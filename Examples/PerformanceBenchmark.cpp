#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Shared/MessageQueue/MessageQueue.h"
#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

using namespace Helianthus::MessageQueue;

int main(int argc, char** argv)
{
    Helianthus::Common::Logger::LoggerConfig LogConfig;
    LogConfig.Level = Helianthus::Common::LogLevel::WARN;
    LogConfig.EnableConsole = true;
    LogConfig.EnableFile = false;
    LogConfig.UseAsync = false;
    Helianthus::Common::Logger::Initialize(LogConfig);

    const std::string QueueName = "performance_benchmark_queue";
    uint32_t MessageCount = 20000;
    uint32_t PayloadBytes = 512;
    std::string Mode = "baseline"; // baseline|compress|encrypt|batch|zero
    uint32_t Runs = 5; // 采样次数

    if (argc >= 2) MessageCount = static_cast<uint32_t>(std::stoul(argv[1]));
    if (argc >= 3) PayloadBytes = static_cast<uint32_t>(std::stoul(argv[2]));
    if (argc >= 4) Mode = argv[3];
    if (argc >= 5) Runs = static_cast<uint32_t>(std::stoul(argv[4]));

    auto Queue = std::make_unique<MessageQueue>();
    if (Queue->Initialize() != QueueResult::SUCCESS)
    {
        std::cerr << "初始化消息队列失败" << std::endl;
        return 1;
    }

    QueueConfig Config;
    Config.Name = QueueName;
    Config.Type = QueueType::STANDARD;
    Config.Persistence = PersistenceMode::MEMORY_ONLY;
    Config.EnableBatching = (Mode == "batch");
    Config.MaxSize = MessageCount * 2;
    Config.MaxSizeBytes = static_cast<uint64_t>(MessageCount) * PayloadBytes * 2ull;
    if (Queue->CreateQueue(Config) != QueueResult::SUCCESS)
    {
        std::cerr << "创建队列失败" << std::endl;
        return 1;
    }

    // 配置压缩/加密（基于模式）
    if (Mode == "compress")
    {
        CompressionConfig Cfg;
        Cfg.Algorithm = CompressionAlgorithm::GZIP;
        Cfg.Level = 6;
        Cfg.MinSize = 256;
        Cfg.EnableAutoCompression = true;
        Queue->SetCompressionConfig(QueueName, Cfg);
    }
    else if (Mode == "encrypt")
    {
        EncryptionConfig Cfg;
        Cfg.Algorithm = EncryptionAlgorithm::AES_256_GCM; // 目前走轻量CTR管道
        Cfg.EnableAutoEncryption = true;
        Cfg.Key.assign(32, '\0');
        Cfg.IV.assign(16, '\0');
        Queue->SetEncryptionConfig(QueueName, Cfg);
    }

    std::string Payload(PayloadBytes, 'x');

    auto RunOnce = [&](void) -> double {
        // 清理上次采样残留，避免队列满
        (void)Queue->PurgeQueue(QueueName);
        auto T0 = std::chrono::steady_clock::now();
        if (Mode == "zero")
        {
            ZeroCopyBuffer Buffer;
            Queue->CreateZeroCopyBuffer(Payload.data(), Payload.size(), Buffer);
            for (uint32_t Index = 0; Index < MessageCount; ++Index)
            {
                if (Queue->SendMessageZeroCopy(QueueName, Buffer) != QueueResult::SUCCESS)
                {
                    Queue->ReleaseZeroCopyBuffer(Buffer);
                    return -1.0;
                }
            }
            Queue->ReleaseZeroCopyBuffer(Buffer);
        }
        else if (Mode == "batch")
        {
            uint32_t BatchId = 0;
            Queue->CreateBatchForQueue(QueueName, BatchId);
            for (uint32_t Index = 0; Index < MessageCount; ++Index)
            {
                auto Msg = std::make_shared<Message>(MessageType::TEXT, Payload);
                Msg->Header.Priority = MessagePriority::NORMAL;
                Msg->Header.Delivery = DeliveryMode::AT_MOST_ONCE;
                Queue->AddToBatch(BatchId, Msg);
            }
            Queue->CommitBatch(BatchId);
        }
        else
        {
            for (uint32_t Index = 0; Index < MessageCount; ++Index)
            {
                auto Msg = std::make_shared<Message>(MessageType::TEXT, Payload);
                Msg->Header.Priority = MessagePriority::NORMAL;
                Msg->Header.Delivery = DeliveryMode::AT_MOST_ONCE;
                if (Queue->SendMessage(QueueName, Msg) != QueueResult::SUCCESS)
                {
                    return -1.0;
                }
            }
        }
        auto T1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(T1 - T0).count();
    };

    std::vector<double> SamplesMs;
    SamplesMs.reserve(Runs);
    // 预热一次
    (void)RunOnce();
    for (uint32_t r = 0; r < Runs; ++r)
    {
        double ms = RunOnce();
        if (ms < 0.0) { std::cerr << "发送失败" << std::endl; return 2; }
        SamplesMs.push_back(ms);
    }

    // 统计
    std::vector<double> Sorted = SamplesMs;
    std::sort(Sorted.begin(), Sorted.end());
    auto Pct = [&](double p) -> double {
        if (Sorted.empty()) return 0.0;
        double idx = p * (Sorted.size() - 1);
        size_t lo = static_cast<size_t>(idx);
        size_t hi = std::min(lo + 1, Sorted.size() - 1);
        double frac = idx - lo;
        return Sorted[lo] * (1.0 - frac) + Sorted[hi] * frac;
    };
    double Sum = 0.0;
    for (double v : SamplesMs) Sum += v;
    double AvgMs = SamplesMs.empty() ? 0.0 : (Sum / SamplesMs.size());
    double P50Ms = Pct(0.50);
    double P95Ms = Pct(0.95);

    auto ToRps = [&](double ms) -> double { return (ms > 0.0) ? (MessageCount / (ms / 1000.0)) : 0.0; };
    double AvgRps = ToRps(AvgMs);
    double P50Rps = ToRps(P50Ms);
    double P95Rps = ToRps(P95Ms);

    // 标准化输出：单行 key=value，便于采集
    std::cout
        << "mode=" << Mode
        << " message_count=" << MessageCount
        << " payload_bytes=" << PayloadBytes
        << " runs=" << Runs
        << " avg_ms=" << AvgMs
        << " p50_ms=" << P50Ms
        << " p95_ms=" << P95Ms
        << " avg_rps=" << AvgRps
        << " p50_rps=" << P50Rps
        << " p95_rps=" << P95Rps
        << "\n";

    return 0;
}


