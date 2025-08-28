#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Helianthus::Common
{
    // 资源使用率统计
    struct ResourceUsageStats
    {
        // CPU 统计
        double CpuUsagePercent = 0.0;           // CPU 使用率百分比
        double CpuLoadAverage1Min = 0.0;        // 1分钟平均负载
        double CpuLoadAverage5Min = 0.0;        // 5分钟平均负载
        double CpuLoadAverage15Min = 0.0;       // 15分钟平均负载
        uint32_t CpuCoreCount = 0;              // CPU 核心数
        
        // 内存统计
        uint64_t TotalMemoryBytes = 0;          // 总内存字节数
        uint64_t UsedMemoryBytes = 0;           // 已使用内存字节数
        uint64_t AvailableMemoryBytes = 0;      // 可用内存字节数
        double MemoryUsagePercent = 0.0;        // 内存使用率百分比
        uint64_t SwapTotalBytes = 0;            // 交换分区总大小
        uint64_t SwapUsedBytes = 0;             // 交换分区已使用大小
        double SwapUsagePercent = 0.0;          // 交换分区使用率
        
        // 磁盘统计
        struct DiskStats
        {
            std::string MountPoint;             // 挂载点
            std::string DeviceName;             // 设备名
            uint64_t TotalBytes = 0;            // 总空间
            uint64_t UsedBytes = 0;             // 已使用空间
            uint64_t AvailableBytes = 0;        // 可用空间
            double UsagePercent = 0.0;          // 使用率百分比
            uint64_t ReadBytesPerSec = 0;       // 每秒读取字节数
            uint64_t WriteBytesPerSec = 0;      // 每秒写入字节数
            uint64_t ReadCountPerSec = 0;       // 每秒读取次数
            uint64_t WriteCountPerSec = 0;      // 每秒写入次数
        };
        std::vector<DiskStats> DiskStatsList;   // 磁盘统计列表
        
        // 网络统计
        struct NetworkStats
        {
            std::string InterfaceName;          // 网络接口名
            uint64_t BytesReceived = 0;         // 接收字节数
            uint64_t BytesSent = 0;             // 发送字节数
            uint64_t PacketsReceived = 0;       // 接收包数
            uint64_t PacketsSent = 0;           // 发送包数
            uint64_t BytesReceivedPerSec = 0;   // 每秒接收字节数
            uint64_t BytesSentPerSec = 0;       // 每秒发送字节数
            uint64_t PacketsReceivedPerSec = 0; // 每秒接收包数
            uint64_t PacketsSentPerSec = 0;     // 每秒发送包数
            uint64_t ErrorsReceived = 0;        // 接收错误数
            uint64_t ErrorsSent = 0;            // 发送错误数
            uint64_t DroppedReceived = 0;       // 接收丢弃数
            uint64_t DroppedSent = 0;           // 发送丢弃数
        };
        std::vector<NetworkStats> NetworkStatsList; // 网络统计列表
        
        // 时间戳
        std::chrono::system_clock::time_point Timestamp = std::chrono::system_clock::now();
        
        // 辅助方法
        double GetMemoryUsagePercent() const 
        { 
            return TotalMemoryBytes > 0 ? (static_cast<double>(UsedMemoryBytes) / TotalMemoryBytes) * 100.0 : 0.0; 
        }
        
        double GetSwapUsagePercent() const 
        { 
            return SwapTotalBytes > 0 ? (static_cast<double>(SwapUsedBytes) / SwapTotalBytes) * 100.0 : 0.0; 
        }
    };
    
    // 资源监控配置
    struct ResourceMonitorConfig
    {
        uint32_t SamplingIntervalMs = 5000;     // 采样间隔（毫秒）
        uint32_t HistoryWindowMs = 300000;      // 历史窗口（5分钟）
        bool EnableCpuMonitoring = true;        // 启用CPU监控
        bool EnableMemoryMonitoring = true;     // 启用内存监控
        bool EnableDiskMonitoring = true;       // 启用磁盘监控
        bool EnableNetworkMonitoring = true;    // 启用网络监控
        std::vector<std::string> DiskMountPoints; // 要监控的磁盘挂载点（空表示监控所有）
        std::vector<std::string> NetworkInterfaces; // 要监控的网络接口（空表示监控所有）
        std::function<void(const ResourceUsageStats&)> OnStatsUpdate; // 统计更新回调
    };
    
    // 资源监控器接口
    class IResourceMonitor
    {
    public:
        virtual ~IResourceMonitor() = default;
        
        // 初始化和清理
        virtual bool Initialize(const ResourceMonitorConfig& Config) = 0;
        virtual void Shutdown() = 0;
        
        // 启动和停止监控
        virtual bool StartMonitoring() = 0;
        virtual void StopMonitoring() = 0;
        
        // 获取统计信息
        virtual ResourceUsageStats GetCurrentStats() const = 0;
        virtual std::vector<ResourceUsageStats> GetHistoryStats() const = 0;
        
        // 配置管理
        virtual void UpdateConfig(const ResourceMonitorConfig& Config) = 0;
        virtual ResourceMonitorConfig GetConfig() const = 0;
        
        // 重置统计
        virtual void ResetStats() = 0;
    };
    
    // 资源监控器实现
    class ResourceMonitor : public IResourceMonitor
    {
    public:
        ResourceMonitor();
        ~ResourceMonitor();
        
        // IResourceMonitor 接口实现
        bool Initialize(const ResourceMonitorConfig& Config) override;
        void Shutdown() override;
        bool StartMonitoring() override;
        void StopMonitoring() override;
        ResourceUsageStats GetCurrentStats() const override;
        std::vector<ResourceUsageStats> GetHistoryStats() const override;
        void UpdateConfig(const ResourceMonitorConfig& Config) override;
        ResourceMonitorConfig GetConfig() const override;
        void ResetStats() override;
        
    private:
        // 监控线程函数
        void MonitoringThreadFunc();
        
        // 资源采集函数
        void CollectCpuStats(ResourceUsageStats& Stats);
        void CollectMemoryStats(ResourceUsageStats& Stats);
        void CollectDiskStats(ResourceUsageStats& Stats);
        void CollectNetworkStats(ResourceUsageStats& Stats);
        
        // 平台特定的实现
        void CollectCpuStatsImpl(ResourceUsageStats& Stats);
        void CollectMemoryStatsImpl(ResourceUsageStats& Stats);
        void CollectDiskStatsImpl(ResourceUsageStats& Stats);
        void CollectNetworkStatsImpl(ResourceUsageStats& Stats);
        
        // 历史数据管理
        void AddToHistory(const ResourceUsageStats& Stats);
        void CleanupOldHistory();
        
        // 成员变量
        ResourceMonitorConfig Config;
        std::atomic<bool> IsInitialized{false};
        std::atomic<bool> IsMonitoring{false};
        std::atomic<bool> ShouldStop{false};
        
        mutable std::mutex StatsMutex;
        ResourceUsageStats CurrentStats;
        std::vector<ResourceUsageStats> HistoryStats;
        
        std::thread MonitoringThread;
        std::condition_variable MonitoringCondition;
        
        // 网络统计的上一帧数据（用于计算速率）
        std::vector<ResourceUsageStats::NetworkStats> PreviousNetworkStats;
        std::chrono::system_clock::time_point LastNetworkUpdate;
        
        // 磁盘统计的上一帧数据（用于计算速率）
        std::vector<ResourceUsageStats::DiskStats> PreviousDiskStats;
        std::chrono::system_clock::time_point LastDiskUpdate;
    };
    
    // 全局资源监控器实例
    ResourceMonitor& GetResourceMonitor();
    
    // 便捷宏
    #define RESOURCE_MONITOR() Helianthus::Common::GetResourceMonitor()
}
