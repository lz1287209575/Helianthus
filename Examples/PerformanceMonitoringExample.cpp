#include "Shared/Network/Asio/PerformanceMetrics.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace Helianthus::Network::Asio;

int main()
{
    std::cout << "=== Helianthus 性能监控系统示例 ===\n\n";
    
    auto& Monitor = PerformanceMonitor::Instance();
    
    // 1. 注册连接和操作
    std::cout << "1. 注册连接和操作...\n";
    Monitor.RegisterConnection("web_server", "127.0.0.1:8080");
    Monitor.RegisterConnection("db_connection", "127.0.0.1:3306");
    Monitor.RegisterOperation("http_request", "send", "tcp");
    Monitor.RegisterOperation("db_query", "query", "tcp");
    
    // 2. 模拟网络操作
    std::cout << "2. 模拟网络操作...\n";
    std::random_device Rd;
    std::mt19937 Gen(Rd());
    std::normal_distribution<> LatencyDist(5000000, 1000000); // 5ms ± 1ms
    std::uniform_int_distribution<> ErrorDist(1, 100);
    
    for (int I = 0; I < 1000; ++I) {
        // 模拟HTTP请求
        uint64_t HttpLatency = std::max(100000L, static_cast<long>(LatencyDist(Gen)));
        bool HttpSuccess = ErrorDist(Gen) > 5; // 95% 成功率
        Monitor.UpdateConnectionMetrics("web_server", HttpSuccess, HttpLatency, 1024);
        Monitor.UpdateOperationMetrics("http_request", HttpSuccess, HttpLatency, 1024);
        
        // 模拟数据库查询
        uint64_t DbLatency = std::max(50000L, static_cast<long>(LatencyDist(Gen) * 0.3));
        bool DbSuccess = ErrorDist(Gen) > 2; // 98% 成功率
        Monitor.UpdateConnectionMetrics("db_connection", DbSuccess, DbLatency, 512);
        Monitor.UpdateOperationMetrics("db_query", DbSuccess, DbLatency, 512);
        
        // 模拟错误分类
        if (!HttpSuccess) {
            std::string ErrorType = (ErrorDist(Gen) % 4 == 0) ? "timeout" : "network";
            Monitor.UpdateErrorStats("web_server", ErrorType);
            Monitor.UpdateOperationErrorStats("http_request", ErrorType);
        }
        
        if (!DbSuccess) {
            Monitor.UpdateErrorStats("db_connection", "protocol");
            Monitor.UpdateOperationErrorStats("db_query", "protocol");
        }
        
        if (I % 100 == 0) {
            std::cout << "  处理了 " << I << " 个操作...\n";
        }
    }
    
    // 3. 设置连接池统计
    std::cout << "3. 设置连接池统计...\n";
    ConnectionPoolStats PoolStats;
    PoolStats.TotalPoolSize = 50;
    PoolStats.ActiveConnections = 25;
    PoolStats.IdleConnections = 20;
    PoolStats.MaxConnections = 100;
    PoolStats.ConnectionWaitTimeMs = 5000;
    PoolStats.ConnectionWaitCount = 10;
    PoolStats.PoolExhaustionCount = 2;
    
    Monitor.UpdateConnectionPoolStats("db_connection", PoolStats);
    
    // 4. 设置系统资源统计
    std::cout << "4. 设置系统资源统计...\n";
    ResourceUsageStats ResourceStats;
    ResourceStats.MemoryUsageBytes = 51200000;      // 50MB
    ResourceStats.PeakMemoryUsageBytes = 76800000;  // 75MB
    ResourceStats.ThreadCount = 16;
    ResourceStats.CpuUsagePercent = 35;
    ResourceStats.FileDescriptorCount = 200;
    ResourceStats.BufferPoolUsage = 1000;
    ResourceStats.BufferPoolCapacity = 2000;
    
    Monitor.UpdateResourceUsage(ResourceStats);
    
    // 5. 显示性能指标
    std::cout << "5. 性能指标摘要:\n";
    std::cout << "   ================================\n";
    
    const auto& ConnectionMetrics = Monitor.GetConnectionMetrics();
    const auto& OperationMetrics = Monitor.GetOperationMetrics();
    
    // Web服务器连接指标
    auto WebServer = ConnectionMetrics.at("web_server").get();
    std::cout << "  Web服务器连接:\n";
    std::cout << "    总操作数: " << WebServer->TotalOperations.load() << "\n";
    std::cout << "    成功率: " << std::fixed << std::setprecision(2) 
              << (WebServer->GetSuccessRate() * 100) << "%\n";
    std::cout << "    平均延迟: " << WebServer->GetAverageLatencyMs() << " ms\n";
    std::cout << "    P50延迟: " << WebServer->GetLatencyPercentileMs(0.50) << " ms\n";
    std::cout << "    P95延迟: " << WebServer->GetLatencyPercentileMs(0.95) << " ms\n";
    std::cout << "    P99延迟: " << WebServer->GetLatencyPercentileMs(0.99) << " ms\n";
    std::cout << "    吞吐量: " << WebServer->GetThroughputOpsPerSec() << " ops/sec\n";
    
    // 数据库连接指标
    auto DbConnection = ConnectionMetrics.at("db_connection").get();
    std::cout << "  数据库连接:\n";
    std::cout << "    总操作数: " << DbConnection->TotalOperations.load() << "\n";
    std::cout << "    成功率: " << std::fixed << std::setprecision(2) 
              << (DbConnection->GetSuccessRate() * 100) << "%\n";
    std::cout << "    平均延迟: " << DbConnection->GetAverageLatencyMs() << " ms\n";
    std::cout << "    连接池利用率: " << (DbConnection->PoolStats.GetPoolUtilization() * 100) << "%\n";
    std::cout << "    平均等待时间: " << DbConnection->PoolStats.GetAverageWaitTimeMs() << " ms\n";
    
    // 操作指标
    auto HttpRequest = OperationMetrics.at("http_request").get();
    std::cout << "  HTTP请求操作:\n";
    std::cout << "    总操作数: " << HttpRequest->TotalOperations.load() << "\n";
    std::cout << "    成功率: " << std::fixed << std::setprecision(2) 
              << (HttpRequest->GetSuccessRate() * 100) << "%\n";
    std::cout << "    平均延迟: " << HttpRequest->GetAverageLatencyMs() << " ms\n";
    
    auto DbQuery = OperationMetrics.at("db_query").get();
    std::cout << "  数据库查询操作:\n";
    std::cout << "    总操作数: " << DbQuery->TotalOperations.load() << "\n";
    std::cout << "    成功率: " << std::fixed << std::setprecision(2) 
              << (DbQuery->GetSuccessRate() * 100) << "%\n";
    std::cout << "    平均延迟: " << DbQuery->GetAverageLatencyMs() << " ms\n";
    
    // 错误统计
    std::cout << "  错误统计:\n";
    std::cout << "    Web服务器网络错误: " << WebServer->ErrorStatistics.NetworkErrors.load() << "\n";
    std::cout << "    Web服务器超时错误: " << WebServer->ErrorStatistics.TimeoutErrors.load() << "\n";
    std::cout << "    数据库协议错误: " << DbConnection->ErrorStatistics.ProtocolErrors.load() << "\n";
    
    // 系统资源统计
    auto SystemMetrics = Monitor.GetSystemMetrics();
    std::cout << "  系统资源:\n";
    std::cout << "    内存使用: " << (SystemMetrics.ResourceStats.MemoryUsageBytes / 1024 / 1024) << " MB\n";
    std::cout << "    峰值内存: " << (SystemMetrics.ResourceStats.PeakMemoryUsageBytes / 1024 / 1024) << " MB\n";
    std::cout << "    线程数: " << SystemMetrics.ResourceStats.ThreadCount.load() << "\n";
    std::cout << "    CPU使用率: " << SystemMetrics.ResourceStats.CpuUsagePercent.load() << "%\n";
    std::cout << "    文件描述符: " << SystemMetrics.ResourceStats.FileDescriptorCount.load() << "\n";
    std::cout << "    缓冲区池利用率: " << (SystemMetrics.ResourceStats.GetBufferPoolUtilization() * 100) << "%\n";
    
    // 6. 导出Prometheus格式
    std::cout << "\n6. Prometheus格式导出 (前几行):\n";
    std::cout << "   ================================\n";
    std::string PrometheusData = Monitor.ExportPrometheusMetrics();
    
    // 只显示前几行
    std::istringstream Iss(PrometheusData);
    std::string Line;
    int LineCount = 0;
    while (std::getline(Iss, Line) && LineCount < 20) {
        std::cout << "  " << Line << "\n";
        LineCount++;
    }
    if (LineCount >= 20) {
        std::cout << "  ... (还有更多指标)\n";
    }
    
    std::cout << "\n=== 示例完成 ===\n";
    return 0;
}
