#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Monitoring/EnhancedPrometheusExporter.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 启动监控
    auto Exporter = std::make_unique<Helianthus::Monitoring::EnhancedPrometheusExporter>();
    Exporter->Start("0.0.0.0", 9090);
    std::cout << "监控服务启动在 http://localhost:9090/metrics" << std::endl;
    
    // 创建队列并产生一些活动
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();
    
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "monitor_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Queue->CreateQueue(Config);
    
    // 模拟一些活动
    for (int i = 0; i < 10; ++i) {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "Monitor message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());
        
        Queue->SendMessage(Config.Name, Message);
        
        // 获取性能统计
        Helianthus::MessageQueue::PerformanceStats Stats;
        Queue->GetPerformanceStats(Stats);
        
        std::cout << "消息 " << i << " - 平均批处理时间: " 
                  << Stats.AverageBatchTimeMs << "ms" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n监控数据已生成，请访问 http://localhost:9090/metrics" << std::endl;
    std::cout << "按 Ctrl+C 退出..." << std::endl;
    
    // 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
