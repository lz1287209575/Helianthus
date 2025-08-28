#include "ResourceMonitor.h"
#include "Logger.h"
#include "LogCategory.h"
#include "LogCategories.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

// 平台特定的头文件
#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <pdh.h>
    #pragma comment(lib, "pdh.lib")
#elif defined(__linux__)
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <net/if.h>
    #include <unistd.h>
    #include <fstream>
    #include <dirent.h>
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <sys/statvfs.h>
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <mach/mach_init.h>
    #include <net/if.h>
    #include <ifaddrs.h>
    #include <net/if_dl.h>
    #include <net/route.h>
#endif

namespace Helianthus::Common
{
    // 前向声明辅助函数
    void CollectDiskSpaceStats(const std::string& MountPoint, ResourceUsageStats& Stats);
    void CollectAllNetworkStats(ResourceUsageStats& Stats);
    void CollectNetworkInterfaceStats(const std::string& InterfaceName, ResourceUsageStats& Stats);
    
    // 全局资源监控器实例
    static std::unique_ptr<ResourceMonitor> GlobalResourceMonitor = nullptr;
    static std::mutex GlobalResourceMonitorMutex;
    
    ResourceMonitor& GetResourceMonitor()
    {
        std::lock_guard<std::mutex> Lock(GlobalResourceMonitorMutex);
        if (!GlobalResourceMonitor)
        {
            GlobalResourceMonitor = std::make_unique<ResourceMonitor>();
        }
        return *GlobalResourceMonitor;
    }
    
    // ResourceMonitor 实现
    ResourceMonitor::ResourceMonitor()
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器创建");
    }
    
    ResourceMonitor::~ResourceMonitor()
    {
        Shutdown();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器销毁");
    }
    
    bool ResourceMonitor::Initialize(const ResourceMonitorConfig& Config)
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        
        if (IsInitialized.load())
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "资源监控器已经初始化");
            return true;
        }
        
        this->Config = Config;
        IsInitialized.store(true);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器初始化完成，采样间隔: {}ms", Config.SamplingIntervalMs);
        return true;
    }
    
    void ResourceMonitor::Shutdown()
    {
        StopMonitoring();
        
        std::lock_guard<std::mutex> Lock(StatsMutex);
        IsInitialized.store(false);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器已关闭");
    }
    
    bool ResourceMonitor::StartMonitoring()
    {
        if (!IsInitialized.load())
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "资源监控器未初始化");
            return false;
        }
        
        if (IsMonitoring.load())
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Warning, "资源监控器已在运行");
            return true;
        }
        
        ShouldStop.store(false);
        IsMonitoring.store(true);
        
        MonitoringThread = std::thread(&ResourceMonitor::MonitoringThreadFunc, this);
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器启动");
        return true;
    }
    
    void ResourceMonitor::StopMonitoring()
    {
        if (!IsMonitoring.load())
        {
            return;
        }
        
        ShouldStop.store(true);
        MonitoringCondition.notify_all();
        
        if (MonitoringThread.joinable())
        {
            MonitoringThread.join();
        }
        
        IsMonitoring.store(false);
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器停止");
    }
    
    ResourceUsageStats ResourceMonitor::GetCurrentStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        return CurrentStats;
    }
    
    std::vector<ResourceUsageStats> ResourceMonitor::GetHistoryStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        return HistoryStats;
    }
    
    void ResourceMonitor::UpdateConfig(const ResourceMonitorConfig& Config)
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        this->Config = Config;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器配置已更新");
    }
    
    ResourceMonitorConfig ResourceMonitor::GetConfig() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        return Config;
    }
    
    void ResourceMonitor::ResetStats()
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        CurrentStats = ResourceUsageStats{};
        HistoryStats.clear();
        PreviousNetworkStats.clear();
        PreviousDiskStats.clear();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控器统计已重置");
    }
    
    void ResourceMonitor::MonitoringThreadFunc()
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控线程启动");
        
        while (!ShouldStop.load())
        {
            auto StartTime = std::chrono::steady_clock::now();
            
            // 采集资源统计
            ResourceUsageStats NewStats;
            NewStats.Timestamp = std::chrono::system_clock::now();
            
            if (Config.EnableCpuMonitoring)
            {
                CollectCpuStats(NewStats);
            }
            
            if (Config.EnableMemoryMonitoring)
            {
                CollectMemoryStats(NewStats);
            }
            
            if (Config.EnableDiskMonitoring)
            {
                CollectDiskStats(NewStats);
            }
            
            if (Config.EnableNetworkMonitoring)
            {
                CollectNetworkStats(NewStats);
            }
            
            // 更新当前统计和历史数据
            {
                std::lock_guard<std::mutex> Lock(StatsMutex);
                CurrentStats = NewStats;
                AddToHistory(NewStats);
            }
            
            // 调用回调函数
            if (Config.OnStatsUpdate)
            {
                try
                {
                    Config.OnStatsUpdate(NewStats);
                }
                catch (const std::exception& e)
                {
                    H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "资源监控回调函数异常: {}", e.what());
                }
            }
            
            // 等待下一次采样
            auto EndTime = std::chrono::steady_clock::now();
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
            auto SleepTime = std::chrono::milliseconds(Config.SamplingIntervalMs) - Elapsed;
            
            if (SleepTime.count() > 0)
            {
                std::unique_lock<std::mutex> Lock(StatsMutex);
                MonitoringCondition.wait_for(Lock, SleepTime, [this] { return ShouldStop.load(); });
            }
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Log, "资源监控线程退出");
    }
    
    void ResourceMonitor::CollectCpuStats(ResourceUsageStats& Stats)
    {
        CollectCpuStatsImpl(Stats);
    }
    
    void ResourceMonitor::CollectMemoryStats(ResourceUsageStats& Stats)
    {
        CollectMemoryStatsImpl(Stats);
    }
    
    void ResourceMonitor::CollectDiskStats(ResourceUsageStats& Stats)
    {
        CollectDiskStatsImpl(Stats);
    }
    
    void ResourceMonitor::CollectNetworkStats(ResourceUsageStats& Stats)
    {
        CollectNetworkStatsImpl(Stats);
    }
    
    void ResourceMonitor::AddToHistory(const ResourceUsageStats& Stats)
    {
        HistoryStats.push_back(Stats);
        CleanupOldHistory();
    }
    
    void ResourceMonitor::CleanupOldHistory()
    {
        auto Now = std::chrono::system_clock::now();
        auto WindowDuration = std::chrono::milliseconds(Config.HistoryWindowMs);
        
        HistoryStats.erase(
            std::remove_if(HistoryStats.begin(), HistoryStats.end(),
                [Now, WindowDuration](const ResourceUsageStats& Stats) {
                    return (Now - Stats.Timestamp) > WindowDuration;
                }),
            HistoryStats.end()
        );
    }
    
    // 平台特定的 CPU 统计实现
    void ResourceMonitor::CollectCpuStatsImpl(ResourceUsageStats& Stats)
    {
#ifdef _WIN32
        // Windows 实现
        static ULARGE_INTEGER LastIdleTime = {0};
        static ULARGE_INTEGER LastKernelTime = {0};
        static ULARGE_INTEGER LastUserTime = {0};
        
        FILETIME IdleTime, KernelTime, UserTime;
        if (GetSystemTimes(&IdleTime, &KernelTime, &UserTime))
        {
            ULARGE_INTEGER CurrentIdleTime, CurrentKernelTime, CurrentUserTime;
            CurrentIdleTime.LowPart = IdleTime.dwLowDateTime;
            CurrentIdleTime.HighPart = IdleTime.dwHighDateTime;
            CurrentKernelTime.LowPart = KernelTime.dwLowDateTime;
            CurrentKernelTime.HighPart = KernelTime.dwHighDateTime;
            CurrentUserTime.LowPart = UserTime.dwLowDateTime;
            CurrentUserTime.HighPart = UserTime.dwHighDateTime;
            
            if (LastIdleTime.QuadPart != 0)
            {
                uint64_t IdleDiff = CurrentIdleTime.QuadPart - LastIdleTime.QuadPart;
                uint64_t KernelDiff = CurrentKernelTime.QuadPart - LastKernelTime.QuadPart;
                uint64_t UserDiff = CurrentUserTime.QuadPart - LastUserTime.QuadPart;
                uint64_t TotalDiff = KernelDiff + UserDiff;
                
                if (TotalDiff > 0)
                {
                    Stats.CpuUsagePercent = 100.0 - (static_cast<double>(IdleDiff) / TotalDiff * 100.0);
                }
            }
            
            LastIdleTime = CurrentIdleTime;
            LastKernelTime = CurrentKernelTime;
            LastUserTime = CurrentUserTime;
        }
        
        // 获取 CPU 核心数
        SYSTEM_INFO SysInfo;
        GetSystemInfo(&SysInfo);
        Stats.CpuCoreCount = SysInfo.dwNumberOfProcessors;
        
#elif defined(__linux__)
        // Linux 实现
        std::ifstream StatFile("/proc/stat");
        if (StatFile.is_open())
        {
            std::string Line;
            if (std::getline(StatFile, Line))
            {
                std::istringstream Iss(Line);
                std::string Cpu;
                uint64_t User, Nice, System, Idle, Iowait, Irq, Softirq, Steal, Guest, GuestNice;
                
                Iss >> Cpu >> User >> Nice >> System >> Idle >> Iowait >> Irq >> Softirq >> Steal >> Guest >> GuestNice;
                
                static uint64_t LastTotal = 0, LastIdle = 0;
                uint64_t Total = User + Nice + System + Idle + Iowait + Irq + Softirq + Steal;
                uint64_t IdleTotal = Idle + Iowait;
                
                if (LastTotal != 0)
                {
                    uint64_t TotalDiff = Total - LastTotal;
                    uint64_t IdleDiff = IdleTotal - LastIdle;
                    
                    if (TotalDiff > 0)
                    {
                        Stats.CpuUsagePercent = 100.0 - (static_cast<double>(IdleDiff) / TotalDiff * 100.0);
                    }
                }
                
                LastTotal = Total;
                LastIdle = IdleTotal;
            }
            StatFile.close();
        }
        
        // 获取负载平均值
        std::ifstream LoadAvgFile("/proc/loadavg");
        if (LoadAvgFile.is_open())
        {
            LoadAvgFile >> Stats.CpuLoadAverage1Min >> Stats.CpuLoadAverage5Min >> Stats.CpuLoadAverage15Min;
            LoadAvgFile.close();
        }
        
        // 获取 CPU 核心数
        Stats.CpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
        
#elif defined(__APPLE__)
        // macOS 实现
        host_t Host = mach_host_self();
        mach_msg_type_number_t Count = HOST_CPU_LOAD_INFO_COUNT;
        host_cpu_load_info_data_t CpuLoad;
        
        if (host_statistics(Host, HOST_CPU_LOAD_INFO, (host_info_t)&CpuLoad, &Count) == KERN_SUCCESS)
        {
            static uint64_t LastUser = 0, LastSystem = 0, LastIdle = 0;
            uint64_t User = CpuLoad.cpu_ticks[CPU_STATE_USER];
            uint64_t System = CpuLoad.cpu_ticks[CPU_STATE_SYSTEM];
            uint64_t Idle = CpuLoad.cpu_ticks[CPU_STATE_IDLE];
            
            if (LastUser != 0)
            {
                uint64_t UserDiff = User - LastUser;
                uint64_t SystemDiff = System - LastSystem;
                uint64_t IdleDiff = Idle - LastIdle;
                uint64_t TotalDiff = UserDiff + SystemDiff + IdleDiff;
                
                if (TotalDiff > 0)
                {
                    Stats.CpuUsagePercent = static_cast<double>(UserDiff + SystemDiff) / TotalDiff * 100.0;
                }
            }
            
            LastUser = User;
            LastSystem = System;
            LastIdle = Idle;
        }
        
        // 获取负载平均值
        int Mib[2] = {CTL_VM, VM_LOADAVG};
        struct loadavg LoadAvg;
        size_t Size = sizeof(LoadAvg);
        if (sysctl(Mib, 2, &LoadAvg, &Size, nullptr, 0) == 0)
        {
            // macOS uses fscale instead of ldscale
            long Scale = LoadAvg.fscale;
            if (Scale == 0) Scale = 1;
            Stats.CpuLoadAverage1Min = static_cast<double>(LoadAvg.ldavg[0]) / static_cast<double>(Scale);
            Stats.CpuLoadAverage5Min = static_cast<double>(LoadAvg.ldavg[1]) / static_cast<double>(Scale);
            Stats.CpuLoadAverage15Min = static_cast<double>(LoadAvg.ldavg[2]) / static_cast<double>(Scale);
        }
        
        // 获取 CPU 核心数
        size_t CpuCountSize = sizeof(Stats.CpuCoreCount);
        sysctlbyname("hw.ncpu", &Stats.CpuCoreCount, &CpuCountSize, nullptr, 0);
#endif
    }
    
    // 平台特定的内存统计实现
    void ResourceMonitor::CollectMemoryStatsImpl(ResourceUsageStats& Stats)
    {
#ifdef _WIN32
        // Windows 实现
        MEMORYSTATUSEX MemStatus;
        MemStatus.dwLength = sizeof(MemStatus);
        if (GlobalMemoryStatusEx(&MemStatus))
        {
            Stats.TotalMemoryBytes = MemStatus.ullTotalPhys;
            Stats.AvailableMemoryBytes = MemStatus.ullAvailPhys;
            Stats.UsedMemoryBytes = Stats.TotalMemoryBytes - Stats.AvailableMemoryBytes;
            Stats.MemoryUsagePercent = MemStatus.dwMemoryLoad;
            
            Stats.SwapTotalBytes = MemStatus.ullTotalPageFile;
            Stats.SwapUsedBytes = MemStatus.ullTotalPageFile - MemStatus.ullAvailPageFile;
            Stats.SwapUsagePercent = Stats.SwapTotalBytes > 0 ? 
                (static_cast<double>(Stats.SwapUsedBytes) / Stats.SwapTotalBytes) * 100.0 : 0.0;
        }
        
#elif defined(__linux__)
        // Linux 实现
        std::ifstream MemInfoFile("/proc/meminfo");
        if (MemInfoFile.is_open())
        {
            std::string Line;
            uint64_t MemTotal = 0, MemAvailable = 0, MemFree = 0, Buffers = 0, Cached = 0;
            uint64_t SwapTotal = 0, SwapFree = 0;
            
            while (std::getline(MemInfoFile, Line))
            {
                std::istringstream Iss(Line);
                std::string Key, Value, Unit;
                Iss >> Key >> Value >> Unit;
                
                if (Key == "MemTotal:") MemTotal = std::stoull(Value) * 1024;
                else if (Key == "MemAvailable:") MemAvailable = std::stoull(Value) * 1024;
                else if (Key == "MemFree:") MemFree = std::stoull(Value) * 1024;
                else if (Key == "Buffers:") Buffers = std::stoull(Value) * 1024;
                else if (Key == "Cached:") Cached = std::stoull(Value) * 1024;
                else if (Key == "SwapTotal:") SwapTotal = std::stoull(Value) * 1024;
                else if (Key == "SwapFree:") SwapFree = std::stoull(Value) * 1024;
            }
            
            Stats.TotalMemoryBytes = MemTotal;
            Stats.AvailableMemoryBytes = MemAvailable > 0 ? MemAvailable : (MemFree + Buffers + Cached);
            Stats.UsedMemoryBytes = Stats.TotalMemoryBytes - Stats.AvailableMemoryBytes;
            Stats.MemoryUsagePercent = Stats.GetMemoryUsagePercent();
            
            Stats.SwapTotalBytes = SwapTotal;
            Stats.SwapUsedBytes = SwapTotal - SwapFree;
            Stats.SwapUsagePercent = Stats.GetSwapUsagePercent();
            
            MemInfoFile.close();
        }
        
#elif defined(__APPLE__)
        // macOS 实现
        vm_size_t PageSize;
        host_page_size(mach_host_self(), &PageSize);
        
        vm_statistics64_data_t VmStats;
        mach_msg_type_number_t Count = sizeof(VmStats) / sizeof(natural_t);
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&VmStats, &Count) == KERN_SUCCESS)
        {
            uint64_t TotalMemory = (VmStats.active_count + VmStats.inactive_count + VmStats.wire_count + VmStats.free_count) * PageSize;
            uint64_t UsedMemory = (VmStats.active_count + VmStats.wire_count) * PageSize;
            uint64_t AvailableMemory = (VmStats.inactive_count + VmStats.free_count) * PageSize;
            
            Stats.TotalMemoryBytes = TotalMemory;
            Stats.UsedMemoryBytes = UsedMemory;
            Stats.AvailableMemoryBytes = AvailableMemory;
            Stats.MemoryUsagePercent = Stats.GetMemoryUsagePercent();
        }
        
        // 获取交换分区信息
        xsw_usage SwapUsage;
        size_t Size = sizeof(SwapUsage);
        if (sysctlbyname("vm.swapusage", &SwapUsage, &Size, nullptr, 0) == 0)
        {
            Stats.SwapTotalBytes = SwapUsage.xsu_total;
            Stats.SwapUsedBytes = SwapUsage.xsu_used;
            Stats.SwapUsagePercent = Stats.GetSwapUsagePercent();
        }
#endif
    }
    
    // 平台特定的磁盘统计实现
    void ResourceMonitor::CollectDiskStatsImpl(ResourceUsageStats& Stats)
    {
        // 这里实现磁盘统计，包括空间使用率和 I/O 统计
        // 由于实现较复杂，这里先提供一个基础框架
        
#ifdef _WIN32
        // Windows 磁盘统计实现
        // 使用 WMI 或 Performance Counters 获取磁盘统计
#elif defined(__linux__)
        // Linux 磁盘统计实现
        // 读取 /proc/diskstats 和 /proc/mounts
#elif defined(__APPLE__)
        // macOS 磁盘统计实现
        // 使用 IOKit 或 sysctl 获取磁盘统计
#endif
        
        // 临时实现：获取基本磁盘空间信息
        if (Config.DiskMountPoints.empty())
        {
            // 监控根目录
            CollectDiskSpaceStats("/", Stats);
        }
        else
        {
            for (const auto& MountPoint : Config.DiskMountPoints)
            {
                CollectDiskSpaceStats(MountPoint, Stats);
            }
        }
    }
    
    // 辅助函数：收集磁盘空间统计
    void CollectDiskSpaceStats(const std::string& MountPoint, ResourceUsageStats& Stats)
    {
#ifdef _WIN32
        ULARGE_INTEGER FreeBytesAvailable, TotalBytes, TotalFreeBytes;
        if (GetDiskFreeSpaceExA(MountPoint.c_str(), &FreeBytesAvailable, &TotalBytes, &TotalFreeBytes))
        {
            ResourceUsageStats::DiskStats DiskStats;
            DiskStats.MountPoint = MountPoint;
            DiskStats.TotalBytes = TotalBytes.QuadPart;
            DiskStats.AvailableBytes = FreeBytesAvailable.QuadPart;
            DiskStats.UsedBytes = TotalBytes.QuadPart - FreeBytesAvailable.QuadPart;
            DiskStats.UsagePercent = DiskStats.TotalBytes > 0 ? 
                (static_cast<double>(DiskStats.UsedBytes) / DiskStats.TotalBytes) * 100.0 : 0.0;
            
            Stats.DiskStatsList.push_back(DiskStats);
        }
#elif defined(__linux__) || defined(__APPLE__)
        struct statvfs StatVfs;
        if (statvfs(MountPoint.c_str(), &StatVfs) == 0)
        {
            ResourceUsageStats::DiskStats DiskStats;
            DiskStats.MountPoint = MountPoint;
            DiskStats.TotalBytes = static_cast<uint64_t>(StatVfs.f_blocks) * StatVfs.f_frsize;
            DiskStats.AvailableBytes = static_cast<uint64_t>(StatVfs.f_bavail) * StatVfs.f_frsize;
            DiskStats.UsedBytes = DiskStats.TotalBytes - DiskStats.AvailableBytes;
            DiskStats.UsagePercent = DiskStats.TotalBytes > 0 ? 
                (static_cast<double>(DiskStats.UsedBytes) / DiskStats.TotalBytes) * 100.0 : 0.0;
            
            Stats.DiskStatsList.push_back(DiskStats);
        }
#endif
    }
    
    // 平台特定的网络统计实现
    void ResourceMonitor::CollectNetworkStatsImpl(ResourceUsageStats& Stats)
    {
        // 这里实现网络统计，包括网络接口的流量统计
        // 由于实现较复杂，这里先提供一个基础框架
        
#ifdef _WIN32
        // Windows 网络统计实现
        // 使用 WMI 或 Performance Counters 获取网络统计
#elif defined(__linux__)
        // Linux 网络统计实现
        // 读取 /proc/net/dev
#elif defined(__APPLE__)
        // macOS 网络统计实现
        // 使用 sysctl 获取网络统计
#endif
        
        // 临时实现：获取基本网络接口信息
        if (Config.NetworkInterfaces.empty())
        {
            // 监控所有网络接口
            CollectAllNetworkStats(Stats);
        }
        else
        {
            for (const auto& Interface : Config.NetworkInterfaces)
            {
                CollectNetworkInterfaceStats(Interface, Stats);
            }
        }
    }
    
    // 辅助函数：收集所有网络接口统计
    void CollectAllNetworkStats(ResourceUsageStats& Stats)
    {
#ifdef _WIN32
        // Windows 实现
#elif defined(__linux__)
        // Linux 实现
        std::ifstream NetDevFile("/proc/net/dev");
        if (NetDevFile.is_open())
        {
            std::string Line;
            // 跳过前两行标题
            std::getline(NetDevFile, Line);
            std::getline(NetDevFile, Line);
            
            while (std::getline(NetDevFile, Line))
            {
                std::istringstream Iss(Line);
                std::string InterfaceName;
                uint64_t BytesReceived, PacketsReceived, ErrorsReceived, DroppedReceived;
                uint64_t BytesSent, PacketsSent, ErrorsSent, DroppedSent;
                
                Iss >> InterfaceName >> BytesReceived >> PacketsReceived >> ErrorsReceived >> DroppedReceived
                    >> BytesSent >> PacketsSent >> ErrorsSent >> DroppedSent;
                
                // 移除接口名中的冒号
                if (!InterfaceName.empty() && InterfaceName.back() == ':')
                {
                    InterfaceName.pop_back();
                }
                
                ResourceUsageStats::NetworkStats NetworkStats;
                NetworkStats.InterfaceName = InterfaceName;
                NetworkStats.BytesReceived = BytesReceived;
                NetworkStats.BytesSent = BytesSent;
                NetworkStats.PacketsReceived = PacketsReceived;
                NetworkStats.PacketsSent = PacketsSent;
                NetworkStats.ErrorsReceived = ErrorsReceived;
                NetworkStats.ErrorsSent = ErrorsSent;
                NetworkStats.DroppedReceived = DroppedReceived;
                NetworkStats.DroppedSent = DroppedSent;
                
                Stats.NetworkStatsList.push_back(NetworkStats);
            }
            NetDevFile.close();
        }
#elif defined(__APPLE__)
        // macOS 实现
        struct ifaddrs* IfAddrs;
        if (getifaddrs(&IfAddrs) == 0)
        {
            for (struct ifaddrs* Ifa = IfAddrs; Ifa != nullptr; Ifa = Ifa->ifa_next)
            {
                if (Ifa->ifa_addr == nullptr) continue;
                
                ResourceUsageStats::NetworkStats NetworkStats;
                NetworkStats.InterfaceName = Ifa->ifa_name;
                
                // 这里需要进一步实现获取网络统计的逻辑
                Stats.NetworkStatsList.push_back(NetworkStats);
            }
            freeifaddrs(IfAddrs);
        }
#endif
    }
    
    // 辅助函数：收集特定网络接口统计
    void CollectNetworkInterfaceStats(const std::string& InterfaceName, ResourceUsageStats& Stats)
    {
        // 实现特定网络接口的统计收集
        // 这里可以调用 CollectAllNetworkStats 然后过滤出指定的接口
    }
}
