#include "Common/Logger.h"
#include "Common/LogCategory.h"

using namespace Helianthus::Common;

H_DEFINE_LOG_CATEGORY(Net, LogVerbosity::Log);
H_DEFINE_LOG_CATEGORY(Perf, LogVerbosity::Log);

int main()
{
    Logger::Initialize();

    H_LOG(Net, LogVerbosity::Log, "Server starting on {}:{}", "127.0.0.1", 8080);
    H_LOG(Net, LogVerbosity::Warning, "Accept backlog is high: {}", 128);
    H_LOG(Net, LogVerbosity::Error, "Socket error code={} message={}", 104, "Connection reset by peer");

    H_LOG(Perf, LogVerbosity::Display, "Average latency: {} ms", 1.23);
    H_LOG(Perf, LogVerbosity::Verbose, "Batch size={} throughput={}ops/s", 64, 10500);

    // 提升 Perf 分类详细度，允许 VeryVerbose 输出
    LogCategory::SetCategoryMinVerbosity("Perf", LogVerbosity::VeryVerbose);
    H_LOG(Perf, LogVerbosity::VeryVerbose, "Detail trace id={}", 42);

    Logger::Shutdown();
    return 0;
}
