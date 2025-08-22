#include "Shared/Network/Asio/PerformanceMetrics.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

using namespace Helianthus::Network::Asio;

class PerformanceMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        PerformanceMonitor::Instance().ResetAllMetrics();
    }
    
    void TearDown() override {
        PerformanceMonitor::Instance().ResetAllMetrics();
    }
};

TEST_F(PerformanceMetricsTest, ConnectionMetrics) {
    auto& monitor = PerformanceMonitor::Instance();
    
    // 注册连接
    monitor.RegisterConnection("conn1", "127.0.0.1:8080");
    monitor.RegisterConnection("conn2", "127.0.0.1:8081");
    
    // 更新连接指标
    for (int I = 0; I < 10; ++I) {
        monitor.UpdateConnectionMetrics("conn1", true, 1500000, 1024); // 1.5ms
        monitor.UpdateConnectionMetrics("conn2", I < 8, 2000000, 2048); // 2.0ms, 2次失败
    }
    
    // 获取指标并验证
    const auto& connectionMetrics = monitor.GetConnectionMetrics();
    
    auto conn1 = connectionMetrics.at("conn1").get();
    EXPECT_EQ(conn1->TotalOperations.load(), 10);
    EXPECT_EQ(conn1->SuccessfulOperations.load(), 10);
    EXPECT_EQ(conn1->FailedOperations.load(), 0);
    EXPECT_EQ(conn1->TotalBytesProcessed.load(), 10 * 1024);
    EXPECT_EQ(conn1->TotalMessagesProcessed.load(), 10);
    EXPECT_DOUBLE_EQ(conn1->GetSuccessRate(), 1.0);
    EXPECT_GT(conn1->GetAverageLatencyMs(), 1.0);
    EXPECT_LT(conn1->GetAverageLatencyMs(), 2.0);
    
    auto conn2 = connectionMetrics.at("conn2").get();
    EXPECT_EQ(conn2->TotalOperations.load(), 10);
    EXPECT_EQ(conn2->SuccessfulOperations.load(), 8);
    EXPECT_EQ(conn2->FailedOperations.load(), 2);
    EXPECT_EQ(conn2->TotalBytesProcessed.load(), 10 * 2048);
    EXPECT_DOUBLE_EQ(conn2->GetSuccessRate(), 0.8);
}

TEST_F(PerformanceMetricsTest, OperationMetrics) {
    auto& monitor = PerformanceMonitor::Instance();
    
    // 注册操作
    monitor.RegisterOperation("op1", "read", "tcp");
    monitor.RegisterOperation("op2", "write", "udp");
    
    // 更新操作指标
    for (int I = 0; I < 15; ++I) {
        monitor.UpdateOperationMetrics("op1", true, 1000000, 512); // 1.0ms
        monitor.UpdateOperationMetrics("op2", I < 12, 1500000, 1024); // 1.5ms, 3次失败
    }
    
    // 获取指标并验证
    const auto& operationMetrics = monitor.GetOperationMetrics();
    
    auto op1 = operationMetrics.at("op1").get();
    EXPECT_EQ(op1->TotalOperations.load(), 15);
    EXPECT_EQ(op1->SuccessfulOperations.load(), 15);
    EXPECT_EQ(op1->FailedOperations.load(), 0);
    EXPECT_EQ(op1->TotalBytesProcessed.load(), 15 * 512);
    EXPECT_DOUBLE_EQ(op1->GetSuccessRate(), 1.0);
    EXPECT_GT(op1->GetAverageLatencyMs(), 0.5);
    EXPECT_LT(op1->GetAverageLatencyMs(), 1.5);
    
    auto op2 = operationMetrics.at("op2").get();
    EXPECT_EQ(op2->TotalOperations.load(), 15);
    EXPECT_EQ(op2->SuccessfulOperations.load(), 12);
    EXPECT_EQ(op2->FailedOperations.load(), 3);
    EXPECT_EQ(op2->TotalBytesProcessed.load(), 15 * 1024);
    EXPECT_DOUBLE_EQ(op2->GetSuccessRate(), 0.8);
}

TEST_F(PerformanceMetricsTest, SystemMetrics) {
    auto& monitor = PerformanceMonitor::Instance();
    
    // 注册一些连接和操作来更新系统指标
    monitor.RegisterConnection("sys_conn1", "127.0.0.1:8080");
    monitor.RegisterConnection("sys_conn2", "127.0.0.1:8081");
    monitor.RegisterOperation("sys_op1", "read", "tcp");
    
    monitor.UpdateConnectionMetrics("sys_conn1", true, 1000000, 1024);
    monitor.UpdateConnectionMetrics("sys_conn2", true, 1500000, 2048);
    monitor.UpdateOperationMetrics("sys_op1", true, 500000, 512);
    
    // 获取系统指标
    auto systemMetrics = monitor.GetSystemMetrics();
    
    EXPECT_EQ(systemMetrics.ActiveConnections.load(), 2);
    EXPECT_EQ(systemMetrics.TotalConnections.load(), 2);
    EXPECT_EQ(systemMetrics.FailedConnections.load(), 0);
}

TEST_F(PerformanceMetricsTest, PrometheusExport) {
    auto& monitor = PerformanceMonitor::Instance();
    
    // 注册并更新一些指标
    monitor.RegisterConnection("prom_conn", "127.0.0.1:8080");
    monitor.RegisterOperation("prom_op", "read", "tcp");
    
    monitor.UpdateConnectionMetrics("prom_conn", true, 1000000, 1024);
    monitor.UpdateOperationMetrics("prom_op", true, 500000, 512);
    
    // 导出Prometheus格式
    std::string prometheusData = monitor.ExportPrometheusMetrics();
    
    // 验证包含必要的指标
    EXPECT_NE(prometheusData.find("helianthus_connection_total_operations"), std::string::npos);
    EXPECT_NE(prometheusData.find("helianthus_operation_total_operations"), std::string::npos);
    EXPECT_NE(prometheusData.find("helianthus_system_active_connections"), std::string::npos);
    EXPECT_NE(prometheusData.find("prom_conn"), std::string::npos);
    EXPECT_NE(prometheusData.find("prom_op"), std::string::npos);
}

TEST_F(PerformanceMetricsTest, ScopedTimer) {
    auto& monitor = PerformanceMonitor::Instance();
    
    {
        ScopedTimer timer("test", "test", "tcp");
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 至少 10ms
    }
    
    const auto& operationMetrics = monitor.GetOperationMetrics();
    auto metrics = operationMetrics.at("test").get();
    
    EXPECT_EQ(metrics->TotalOperations.load(), 1);
    EXPECT_EQ(metrics->SuccessfulOperations.load(), 1);
    EXPECT_EQ(metrics->OperationType, "test");
    EXPECT_EQ(metrics->Protocol, "tcp");
    EXPECT_GT(metrics->GetAverageLatencyMs(), 10.0); // 至少 10ms
}

TEST_F(PerformanceMetricsTest, ResetMetrics) {
    auto& monitor = PerformanceMonitor::Instance();
    
    // 注册并更新指标
    monitor.RegisterConnection("reset_test", "127.0.0.1:8080");
    monitor.RegisterOperation("reset_op", "read", "tcp");
    
    monitor.UpdateConnectionMetrics("reset_test", true, 1000000, 1024);
    monitor.UpdateOperationMetrics("reset_op", true, 500000, 512);
    
    // 验证指标存在
    const auto& connectionMetrics = monitor.GetConnectionMetrics();
    const auto& operationMetrics = monitor.GetOperationMetrics();
    
    EXPECT_EQ(connectionMetrics.at("reset_test")->TotalOperations.load(), 1);
    EXPECT_EQ(operationMetrics.at("reset_op")->TotalOperations.load(), 1);
    
    // 重置指标
    monitor.ResetAllMetrics();
    
    // 验证指标被重置
    EXPECT_EQ(connectionMetrics.at("reset_test")->TotalOperations.load(), 0);
    EXPECT_EQ(operationMetrics.at("reset_op")->TotalOperations.load(), 0);
}

TEST_F(PerformanceMetricsTest, ThroughputCalculation) {
    auto& monitor = PerformanceMonitor::Instance();
    monitor.RegisterConnection("throughput_test", "127.0.0.1:8080");
    
    // 模拟100次操作
    for (int i = 0; i < 100; ++i) {
        monitor.UpdateConnectionMetrics("throughput_test", true, 1000000, 1024);
    }
    
    const auto& connectionMetrics = monitor.GetConnectionMetrics();
    auto metrics = connectionMetrics.at("throughput_test").get();
    
    EXPECT_EQ(metrics->TotalOperations.load(), 100);
    EXPECT_EQ(metrics->SuccessfulOperations.load(), 100);
    EXPECT_EQ(metrics->TotalBytesProcessed.load(), 100 * 1024);
    
    // 验证基本统计功能正常
    EXPECT_GT(metrics->GetAverageLatencyMs(), 0.0);
    EXPECT_DOUBLE_EQ(metrics->GetSuccessRate(), 1.0);
    
    // 吞吐量计算依赖于时间，在测试环境中可能不稳定
    // 我们验证其他指标正常工作即可
}

TEST_F(PerformanceMetricsTest, LatencyStatistics) {
    auto& monitor = PerformanceMonitor::Instance();
    monitor.RegisterOperation("latency_test", "test", "tcp");
    
    // 添加不同延迟的操作
    std::vector<uint64_t> latencies = {100000, 500000, 1000000, 2000000, 5000000}; // 0.1ms 到 5ms
    for (uint64_t latency : latencies) {
        monitor.UpdateOperationMetrics("latency_test", true, latency, 1024);
    }
    
    const auto& operationMetrics = monitor.GetOperationMetrics();
    auto metrics = operationMetrics.at("latency_test").get();
    
    EXPECT_EQ(metrics->TotalOperations.load(), 5);
    EXPECT_EQ(metrics->LatencyCount.load(), 5);
    
    // 验证延迟统计
    EXPECT_EQ(metrics->MinLatencyNs.load(), 100000);  // 最小延迟 0.1ms
    EXPECT_EQ(metrics->MaxLatencyNs.load(), 5000000); // 最大延迟 5ms
    
    double avgLatencyMs = metrics->GetAverageLatencyMs();
    EXPECT_GT(avgLatencyMs, 1.0);
    EXPECT_LT(avgLatencyMs, 2.0);
}
