#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/MessageQueue/IMessageQueue.h"
#include "Shared/MessageQueue/MessageQueue.h"

#include <iostream>
#include <string>

using namespace Helianthus::MessageQueue;

int main()
{
    std::cout << "=== 监控告警功能测试开始 ===" << std::endl;

    // 创建消息队列实例
    auto Queue = std::make_unique<MessageQueue>();
    std::cout << "创建消息队列实例" << std::endl;

    // 初始化消息队列
    std::cout << "开始初始化消息队列..." << std::endl;
    auto InitResult = Queue->Initialize();
    if (InitResult != QueueResult::SUCCESS)
    {
        std::cout << "消息队列初始化失败: " << static_cast<int>(InitResult) << std::endl;
        return 1;
    }
    std::cout << "消息队列初始化成功" << std::endl;

    // 创建测试队列
    QueueConfig Config;
    Config.Name = "alert_test_queue";
    Config.MaxSize = 1000;
    Config.MaxSizeBytes = 1024 * 1024 * 100;  // 100MB
    Config.MessageTtlMs = 30000;              // 30秒
    Config.EnableDeadLetter = true;
    Config.EnablePriority = false;
    Config.EnableBatching = false;

    auto CreateResult = Queue->CreateQueue(Config);
    if (CreateResult != QueueResult::SUCCESS)
    {
        std::cout << "创建队列失败: " << static_cast<int>(CreateResult) << std::endl;
        return 1;
    }
    std::cout << "创建队列成功: " << Config.Name << std::endl;

    // 测试1：设置告警配置
    std::cout << "=== 测试1：设置告警配置 ===" << std::endl;

    AlertConfig AlertConfig1;
    AlertConfig1.Type = AlertType::QUEUE_FULL;
    AlertConfig1.Level = AlertLevel::WARNING;
    AlertConfig1.QueueName = Config.Name;
    AlertConfig1.Threshold = 0.8;      // 80% 使用率时告警
    AlertConfig1.DurationMs = 60000;   // 1分钟
    AlertConfig1.CooldownMs = 300000;  // 5分钟冷却
    AlertConfig1.Enabled = true;
    AlertConfig1.Description = "队列使用率过高告警";
    AlertConfig1.NotifyChannels = {"email", "slack"};

    auto SetAlertResult = Queue->SetAlertConfig(AlertConfig1);
    if (SetAlertResult == QueueResult::SUCCESS)
    {
        std::cout << "设置告警配置成功" << std::endl;
    }
    else
    {
        std::cout << "设置告警配置失败: " << static_cast<int>(SetAlertResult) << std::endl;
    }

    // 测试2：设置告警处理器
    std::cout << "=== 测试2：设置告警处理器 ===" << std::endl;

    Queue->SetAlertHandler(
        [](const Alert& Alert)
        {
            std::cout << "收到告警: id=" << Alert.Id << ", type=" << static_cast<int>(Alert.Type)
                      << ", level=" << static_cast<int>(Alert.Level)
                      << ", message=" << Alert.Message << std::endl;
        });

    // 测试3：查询告警配置
    std::cout << "=== 测试3：查询告警配置 ===" << std::endl;

    AlertConfig RetrievedConfig;
    auto GetAlertResult =
        Queue->GetAlertConfig(AlertType::QUEUE_FULL, Config.Name, RetrievedConfig);
    if (GetAlertResult == QueueResult::SUCCESS)
    {
        std::cout << "查询告警配置成功: type=" << static_cast<int>(RetrievedConfig.Type)
                  << ", level=" << static_cast<int>(RetrievedConfig.Level)
                  << ", threshold=" << RetrievedConfig.Threshold << std::endl;
    }

    // 测试4：查询活跃告警
    std::cout << "=== 测试4：查询活跃告警 ===" << std::endl;

    std::vector<Alert> ActiveAlerts;
    auto GetActiveResult = Queue->GetActiveAlerts(ActiveAlerts);
    if (GetActiveResult == QueueResult::SUCCESS)
    {
        std::cout << "查询活跃告警成功: 数量=" << ActiveAlerts.size() << std::endl;
    }

    // 测试5：查询告警统计
    std::cout << "=== 测试5：查询告警统计 ===" << std::endl;

    AlertStats AlertStats;
    auto GetStatsResult = Queue->GetAlertStats(AlertStats);
    if (GetStatsResult == QueueResult::SUCCESS)
    {
        std::cout << "告警统计:" << std::endl;
        std::cout << "  总告警数: " << AlertStats.TotalAlerts << std::endl;
        std::cout << "  活跃告警数: " << AlertStats.ActiveAlerts << std::endl;
        std::cout << "  信息级别: " << AlertStats.InfoAlerts << std::endl;
        std::cout << "  警告级别: " << AlertStats.WarningAlerts << std::endl;
        std::cout << "  错误级别: " << AlertStats.ErrorAlerts << std::endl;
        std::cout << "  严重级别: " << AlertStats.CriticalAlerts << std::endl;
    }

    // 测试6：查询告警历史
    std::cout << "=== 测试6：查询告警历史 ===" << std::endl;

    std::vector<Alert> AlertHistory;
    auto GetHistoryResult = Queue->GetAlertHistory(10, AlertHistory);
    if (GetHistoryResult == QueueResult::SUCCESS)
    {
        std::cout << "查询告警历史成功: 数量=" << AlertHistory.size() << std::endl;
    }

    // 测试7：模拟告警操作
    std::cout << "=== 测试7：模拟告警操作 ===" << std::endl;

    // 模拟确认告警
    auto AckResult = Queue->AcknowledgeAlert(1);
    if (AckResult == QueueResult::SUCCESS)
    {
        std::cout << "确认告警成功" << std::endl;
    }

    // 模拟解决告警
    auto ResolveResult = Queue->ResolveAlert(1, "问题已解决");
    if (ResolveResult == QueueResult::SUCCESS)
    {
        std::cout << "解决告警成功" << std::endl;
    }

    // 清理
    std::cout << "=== 监控告警功能测试完成 ===" << std::endl;

    return 0;
}
