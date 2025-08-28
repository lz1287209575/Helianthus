#include "Shared/Common/ResourceMonitor.h"
#include "Shared/Common/Logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace Helianthus::Common;

// 格式化字节数为可读格式
std::string FormatBytes(uint64_t Bytes)
{
    const char* Units[] = {"B", "KB", "MB", "GB", "TB"};
    int UnitIndex = 0;
    double Size = static_cast<double>(Bytes);
    
    while (Size >= 1024.0 && UnitIndex < 4)
    {
        Size /= 1024.0;
        UnitIndex++;
    }
    
    std::ostringstream Oss;
    Oss << std::fixed << std::setprecision(2) << Size << " " << Units[UnitIndex];
    return Oss.str();
}

// 资源统计更新回调函数
void OnResourceStatsUpdate(const ResourceUsageStats& Stats)
{
    std::cout << "\n=== 资源使用情况更新 ===" << std::endl;
    std::cout << "时间: " << std::chrono::duration_cast<std::chrono::seconds>(
        Stats.Timestamp.time_since_epoch()).count() << "s" << std::endl;
    
    // CPU 信息
    std::cout << "\n--- CPU 信息 ---" << std::endl;
    std::cout << "CPU 使用率: " << std::fixed << std::setprecision(2) 
              << Stats.CpuUsagePercent << "%" << std::endl;
    std::cout << "CPU 核心数: " << Stats.CpuCoreCount << std::endl;
    std::cout << "负载平均值 (1/5/15分钟): " << Stats.CpuLoadAverage1Min << " / "
              << Stats.CpuLoadAverage5Min << " / " << Stats.CpuLoadAverage15Min << std::endl;
    
    // 内存信息
    std::cout << "\n--- 内存信息 ---" << std::endl;
    std::cout << "总内存: " << FormatBytes(Stats.TotalMemoryBytes) << std::endl;
    std::cout << "已使用: " << FormatBytes(Stats.UsedMemoryBytes) << std::endl;
    std::cout << "可用内存: " << FormatBytes(Stats.AvailableMemoryBytes) << std::endl;
    std::cout << "内存使用率: " << std::fixed << std::setprecision(2) 
              << Stats.MemoryUsagePercent << "%" << std::endl;
    
    if (Stats.SwapTotalBytes > 0)
    {
        std::cout << "交换分区: " << FormatBytes(Stats.SwapTotalBytes) << " (已使用: "
                  << FormatBytes(Stats.SwapUsedBytes) << ", " 
                  << std::fixed << std::setprecision(2) << Stats.SwapUsagePercent << "%)" << std::endl;
    }
    
    // 磁盘信息
    if (!Stats.DiskStatsList.empty())
    {
        std::cout << "\n--- 磁盘信息 ---" << std::endl;
        for (const auto& Disk : Stats.DiskStatsList)
        {
            std::cout << "挂载点: " << Disk.MountPoint << std::endl;
            std::cout << "  总空间: " << FormatBytes(Disk.TotalBytes) << std::endl;
            std::cout << "  已使用: " << FormatBytes(Disk.UsedBytes) << std::endl;
            std::cout << "  可用空间: " << FormatBytes(Disk.AvailableBytes) << std::endl;
            std::cout << "  使用率: " << std::fixed << std::setprecision(2) 
                      << Disk.UsagePercent << "%" << std::endl;
            
            if (Disk.ReadBytesPerSec > 0 || Disk.WriteBytesPerSec > 0)
            {
                std::cout << "  读取速率: " << FormatBytes(Disk.ReadBytesPerSec) << "/s" << std::endl;
                std::cout << "  写入速率: " << FormatBytes(Disk.WriteBytesPerSec) << "/s" << std::endl;
            }
        }
    }
    
    // 网络信息
    if (!Stats.NetworkStatsList.empty())
    {
        std::cout << "\n--- 网络信息 ---" << std::endl;
        for (const auto& Net : Stats.NetworkStatsList)
        {
            std::cout << "接口: " << Net.InterfaceName << std::endl;
            std::cout << "  接收: " << FormatBytes(Net.BytesReceived) 
                      << " (" << FormatBytes(Net.BytesReceivedPerSec) << "/s)" << std::endl;
            std::cout << "  发送: " << FormatBytes(Net.BytesSent) 
                      << " (" << FormatBytes(Net.BytesSentPerSec) << "/s)" << std::endl;
            std::cout << "  接收包数: " << Net.PacketsReceived 
                      << " (" << Net.PacketsReceivedPerSec << "/s)" << std::endl;
            std::cout << "  发送包数: " << Net.PacketsSent 
                      << " (" << Net.PacketsSentPerSec << "/s)" << std::endl;
            
            if (Net.ErrorsReceived > 0 || Net.ErrorsSent > 0)
            {
                std::cout << "  接收错误: " << Net.ErrorsReceived << std::endl;
                std::cout << "  发送错误: " << Net.ErrorsSent << std::endl;
            }
        }
    }
    
    std::cout << "\n" << std::string(50, '=') << std::endl;
}

int main()
{
    // 初始化日志系统
    Logger::Initialize();
    
    std::cout << "=== Helianthus 资源监控器演示 ===" << std::endl;
    
    // 获取资源监控器实例
    auto& ResourceMonitor = GetResourceMonitor();
    
    // 配置资源监控器
    ResourceMonitorConfig Config;
    Config.SamplingIntervalMs = 3000;  // 3秒采样间隔
    Config.HistoryWindowMs = 60000;    // 1分钟历史窗口
    Config.EnableCpuMonitoring = true;
    Config.EnableMemoryMonitoring = true;
    Config.EnableDiskMonitoring = true;
    Config.EnableNetworkMonitoring = true;
    Config.OnStatsUpdate = OnResourceStatsUpdate;  // 设置回调函数
    
    // 初始化资源监控器
    if (!ResourceMonitor.Initialize(Config))
    {
        std::cerr << "资源监控器初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "资源监控器初始化成功" << std::endl;
    
    // 启动监控
    if (!ResourceMonitor.StartMonitoring())
    {
        std::cerr << "资源监控器启动失败" << std::endl;
        return 1;
    }
    
    std::cout << "资源监控器启动成功，开始监控系统资源..." << std::endl;
    std::cout << "按 Ctrl+C 停止监控" << std::endl;
    
    // 模拟一些工作负载来产生资源使用
    std::cout << "\n开始模拟工作负载..." << std::endl;
    
    // 创建一些线程来产生 CPU 负载
    std::vector<std::thread> WorkThreads;
    for (int i = 0; i < 4; ++i)
    {
        WorkThreads.emplace_back([i]() {
            std::cout << "工作线程 " << i << " 启动" << std::endl;
            auto StartTime = std::chrono::steady_clock::now();
            
            // 模拟 CPU 密集型工作
            while (std::chrono::steady_clock::now() - StartTime < std::chrono::seconds(30))
            {
                // 简单的计算来产生 CPU 负载
                volatile double Result = 0.0;
                for (int j = 0; j < 1000000; ++j)
                {
                    Result += std::sin(j) * std::cos(j);
                }
            }
            
            std::cout << "工作线程 " << i << " 完成" << std::endl;
        });
    }
    
    // 等待工作线程完成
    for (auto& Thread : WorkThreads)
    {
        Thread.join();
    }
    
    std::cout << "工作负载模拟完成" << std::endl;
    
    // 显示历史统计
    std::cout << "\n=== 历史统计信息 ===" << std::endl;
    auto HistoryStats = ResourceMonitor.GetHistoryStats();
    std::cout << "历史记录数量: " << HistoryStats.size() << std::endl;
    
    if (!HistoryStats.empty())
    {
        auto& LatestStats = HistoryStats.back();
        std::cout << "最新记录时间: " << std::chrono::duration_cast<std::chrono::seconds>(
            LatestStats.Timestamp.time_since_epoch()).count() << "s" << std::endl;
        
        // 计算平均值
        double AvgCpuUsage = 0.0;
        double AvgMemoryUsage = 0.0;
        int ValidCount = 0;
        
        for (const auto& Stats : HistoryStats)
        {
            if (Stats.CpuUsagePercent > 0.0)
            {
                AvgCpuUsage += Stats.CpuUsagePercent;
                AvgMemoryUsage += Stats.MemoryUsagePercent;
                ValidCount++;
            }
        }
        
        if (ValidCount > 0)
        {
            AvgCpuUsage /= ValidCount;
            AvgMemoryUsage /= ValidCount;
            
            std::cout << "平均 CPU 使用率: " << std::fixed << std::setprecision(2) 
                      << AvgCpuUsage << "%" << std::endl;
            std::cout << "平均内存使用率: " << std::fixed << std::setprecision(2) 
                      << AvgMemoryUsage << "%" << std::endl;
        }
    }
    
    // 停止监控
    ResourceMonitor.StopMonitoring();
    std::cout << "\n资源监控器已停止" << std::endl;
    
    // 重置统计
    ResourceMonitor.ResetStats();
    std::cout << "统计信息已重置" << std::endl;
    
    std::cout << "\n=== 资源监控器演示完成 ===" << std::endl;
    
    return 0;
}
