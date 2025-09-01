#include "Shared/Monitoring/EnhancedPrometheusExporter.h"
#include "Shared/Network/Sockets/TcpSocket.h"
#include "Shared/Network/NetworkTypes.h"
#include "Shared/Common/LogCategories.h"
#include "Shared/Common/Logger.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include "ThirdParty/picohttpparser/picohttpparser.h"

namespace Helianthus::Monitoring
{

// 预定义的直方图桶（毫秒）
const std::vector<double> LatencyHistogram::HistogramBuckets = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.5, 5.0, 7.5, 10.0, 25.0, 50.0, 75.0, 100.0
};

// LatencyHistogram 实现
LatencyHistogram::LatencyHistogram(size_t MaxSamples) : MaxSamples(MaxSamples) {}

void LatencyHistogram::AddSample(uint64_t LatencyNs)
{
    std::lock_guard<std::mutex> Lock(SamplesMutex);
    Samples.push_back(LatencyNs);
    if (Samples.size() > MaxSamples)
    {
        Samples.pop_front();
    }
}

double LatencyHistogram::GetPercentile(double Percentile) const
{
    std::lock_guard<std::mutex> Lock(SamplesMutex);
    if (Samples.empty()) return 0.0;
    
    std::vector<uint64_t> SortedSamples(Samples.begin(), Samples.end());
    std::sort(SortedSamples.begin(), SortedSamples.end());
    
    size_t Index = static_cast<size_t>(Percentile * (SortedSamples.size() - 1));
    if (Index >= SortedSamples.size()) Index = SortedSamples.size() - 1;
    
    return static_cast<double>(SortedSamples[Index]);
}

void LatencyHistogram::Reset()
{
    std::lock_guard<std::mutex> Lock(SamplesMutex);
    Samples.clear();
}

std::vector<std::pair<double, uint64_t>> LatencyHistogram::GetHistogramBuckets() const
{
    std::lock_guard<std::mutex> Lock(SamplesMutex);
    std::vector<std::pair<double, uint64_t>> Buckets;
    
    for (double Bucket : HistogramBuckets)
    {
        uint64_t Count = 0;
        uint64_t BucketNs = static_cast<uint64_t>(Bucket * 1'000'000); // 转换为纳秒
        
        for (uint64_t Sample : Samples)
        {
            if (Sample <= BucketNs)
            {
                Count++;
            }
        }
        
        Buckets.emplace_back(Bucket, Count);
    }
    
    return Buckets;
}

// BatchPerformanceStats 实现
void BatchPerformanceStats::AddSample(uint64_t DurationNs, uint64_t MessageCount)
{
    TotalBatches++;
    TotalMessages += MessageCount;
    TotalDurationNs += DurationNs;
    
    uint64_t CurrentMin = MinDurationNs.load();
    while (DurationNs < CurrentMin && !MinDurationNs.compare_exchange_weak(CurrentMin, DurationNs))
    {
        // 重试直到成功更新最小值
    }
    
    uint64_t CurrentMax = MaxDurationNs.load();
    while (DurationNs > CurrentMax && !MaxDurationNs.compare_exchange_weak(CurrentMax, DurationNs))
    {
        // 重试直到成功更新最大值
    }
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr)
    {
        DurationHistogramPtr = std::make_unique<LatencyHistogram>();
    }
    DurationHistogramPtr->AddSample(DurationNs);
}

double BatchPerformanceStats::GetAverageDurationMs() const
{
    uint64_t Total = TotalBatches.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(TotalDurationNs.load()) / (Total * 1'000'000.0);
}

double BatchPerformanceStats::GetP50DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP50() / 1'000'000.0;
}

double BatchPerformanceStats::GetP95DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP95() / 1'000'000.0;
}

double BatchPerformanceStats::GetP99DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP99() / 1'000'000.0;
}

std::vector<std::pair<double, uint64_t>> BatchPerformanceStats::GetDurationHistogram() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return {};
    return DurationHistogramPtr->GetHistogramBuckets();
}

void BatchPerformanceStats::Reset()
{
    TotalBatches = 0;
    TotalMessages = 0;
    TotalDurationNs = 0;
    MinDurationNs = UINT64_MAX;
    MaxDurationNs = 0;
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (DurationHistogramPtr)
    {
        DurationHistogramPtr->Reset();
    }
}

// ZeroCopyPerformanceStats 实现
void ZeroCopyPerformanceStats::AddSample(uint64_t DurationNs)
{
    TotalOperations++;
    TotalDurationNs += DurationNs;
    
    uint64_t CurrentMin = MinDurationNs.load();
    while (DurationNs < CurrentMin && !MinDurationNs.compare_exchange_weak(CurrentMin, DurationNs))
    {
        // 重试直到成功更新最小值
    }
    
    uint64_t CurrentMax = MaxDurationNs.load();
    while (DurationNs > CurrentMax && !MaxDurationNs.compare_exchange_weak(CurrentMax, DurationNs))
    {
        // 重试直到成功更新最大值
    }
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr)
    {
        DurationHistogramPtr = std::make_unique<LatencyHistogram>();
    }
    DurationHistogramPtr->AddSample(DurationNs);
}

double ZeroCopyPerformanceStats::GetAverageDurationMs() const
{
    uint64_t Total = TotalOperations.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(TotalDurationNs.load()) / (Total * 1'000'000.0);
}

double ZeroCopyPerformanceStats::GetP50DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP50() / 1'000'000.0;
}

double ZeroCopyPerformanceStats::GetP95DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP95() / 1'000'000.0;
}

double ZeroCopyPerformanceStats::GetP99DurationMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return 0.0;
    return DurationHistogramPtr->GetP99() / 1'000'000.0;
}

std::vector<std::pair<double, uint64_t>> ZeroCopyPerformanceStats::GetDurationHistogram() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!DurationHistogramPtr) return {};
    return DurationHistogramPtr->GetHistogramBuckets();
}

void ZeroCopyPerformanceStats::Reset()
{
    TotalOperations = 0;
    TotalDurationNs = 0;
    MinDurationNs = UINT64_MAX;
    MaxDurationNs = 0;
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (DurationHistogramPtr)
    {
        DurationHistogramPtr->Reset();
    }
}

// TransactionPerformanceStats 实现
void TransactionPerformanceStats::AddCommitSample(uint64_t DurationNs)
{
    TotalCommitTimeNs += DurationNs;
    
    uint64_t CurrentMin = MinCommitTimeNs.load();
    while (DurationNs < CurrentMin && !MinCommitTimeNs.compare_exchange_weak(CurrentMin, DurationNs))
    {
        // 重试直到成功更新最小值
    }
    
    uint64_t CurrentMax = MaxCommitTimeNs.load();
    while (DurationNs > CurrentMax && !MaxCommitTimeNs.compare_exchange_weak(CurrentMax, DurationNs))
    {
        // 重试直到成功更新最大值
    }
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!CommitTimeHistogramPtr)
    {
        CommitTimeHistogramPtr = std::make_unique<LatencyHistogram>();
    }
    CommitTimeHistogramPtr->AddSample(DurationNs);
}

void TransactionPerformanceStats::AddRollbackSample(uint64_t DurationNs)
{
    TotalRollbackTimeNs += DurationNs;
    
    uint64_t CurrentMin = MinRollbackTimeNs.load();
    while (DurationNs < CurrentMin && !MinRollbackTimeNs.compare_exchange_weak(CurrentMin, DurationNs))
    {
        // 重试直到成功更新最小值
    }
    
    uint64_t CurrentMax = MaxRollbackTimeNs.load();
    while (DurationNs > CurrentMax && !MaxRollbackTimeNs.compare_exchange_weak(CurrentMax, DurationNs))
    {
        // 重试直到成功更新最大值
    }
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!RollbackTimeHistogramPtr)
    {
        RollbackTimeHistogramPtr = std::make_unique<LatencyHistogram>();
    }
    RollbackTimeHistogramPtr->AddSample(DurationNs);
}

void TransactionPerformanceStats::UpdateTransactionCount(bool Committed, bool RolledBack, bool Timeout, bool Failed)
{
    TotalTransactions++;
    if (Committed) CommittedTransactions++;
    if (RolledBack) RolledBackTransactions++;
    if (Timeout) TimeoutTransactions++;
    if (Failed) FailedTransactions++;
}

double TransactionPerformanceStats::GetAverageCommitTimeMs() const
{
    uint64_t Total = CommittedTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(TotalCommitTimeNs.load()) / (Total * 1'000'000.0);
}

double TransactionPerformanceStats::GetP50CommitTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!CommitTimeHistogramPtr) return 0.0;
    return CommitTimeHistogramPtr->GetP50() / 1'000'000.0;
}

double TransactionPerformanceStats::GetP95CommitTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!CommitTimeHistogramPtr) return 0.0;
    return CommitTimeHistogramPtr->GetP95() / 1'000'000.0;
}

double TransactionPerformanceStats::GetP99CommitTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!CommitTimeHistogramPtr) return 0.0;
    return CommitTimeHistogramPtr->GetP99() / 1'000'000.0;
}

double TransactionPerformanceStats::GetAverageRollbackTimeMs() const
{
    uint64_t Total = RolledBackTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(TotalRollbackTimeNs.load()) / (Total * 1'000'000.0);
}

double TransactionPerformanceStats::GetP50RollbackTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!RollbackTimeHistogramPtr) return 0.0;
    return RollbackTimeHistogramPtr->GetP50() / 1'000'000.0;
}

double TransactionPerformanceStats::GetP95RollbackTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!RollbackTimeHistogramPtr) return 0.0;
    return RollbackTimeHistogramPtr->GetP95() / 1'000'000.0;
}

double TransactionPerformanceStats::GetP99RollbackTimeMs() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!RollbackTimeHistogramPtr) return 0.0;
    return RollbackTimeHistogramPtr->GetP99() / 1'000'000.0;
}

std::vector<std::pair<double, uint64_t>> TransactionPerformanceStats::GetCommitTimeHistogram() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!CommitTimeHistogramPtr) return {};
    return CommitTimeHistogramPtr->GetHistogramBuckets();
}

std::vector<std::pair<double, uint64_t>> TransactionPerformanceStats::GetRollbackTimeHistogram() const
{
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (!RollbackTimeHistogramPtr) return {};
    return RollbackTimeHistogramPtr->GetHistogramBuckets();
}

double TransactionPerformanceStats::GetSuccessRate() const
{
    uint64_t Total = TotalTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(CommittedTransactions.load()) / Total;
}

double TransactionPerformanceStats::GetRollbackRate() const
{
    uint64_t Total = TotalTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(RolledBackTransactions.load()) / Total;
}

double TransactionPerformanceStats::GetTimeoutRate() const
{
    uint64_t Total = TotalTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(TimeoutTransactions.load()) / Total;
}

double TransactionPerformanceStats::GetFailureRate() const
{
    uint64_t Total = TotalTransactions.load();
    if (Total == 0) return 0.0;
    return static_cast<double>(FailedTransactions.load()) / Total;
}

void TransactionPerformanceStats::Reset()
{
    TotalTransactions = 0;
    CommittedTransactions = 0;
    RolledBackTransactions = 0;
    TimeoutTransactions = 0;
    FailedTransactions = 0;
    
    TotalCommitTimeNs = 0;
    MinCommitTimeNs = UINT64_MAX;
    MaxCommitTimeNs = 0;
    
    TotalRollbackTimeNs = 0;
    MinRollbackTimeNs = UINT64_MAX;
    MaxRollbackTimeNs = 0;
    
    std::lock_guard<std::mutex> Lock(HistogramMutex);
    if (CommitTimeHistogramPtr)
    {
        CommitTimeHistogramPtr->Reset();
    }
    if (RollbackTimeHistogramPtr)
    {
        RollbackTimeHistogramPtr->Reset();
    }
}

// EnhancedPrometheusExporter 实现
EnhancedPrometheusExporter::EnhancedPrometheusExporter() = default;
EnhancedPrometheusExporter::~EnhancedPrometheusExporter()
{
    Stop();
}

bool EnhancedPrometheusExporter::Start(uint16_t Port, MetricsProvider ProviderFn)
{
    if (Running.load()) return true;
    Provider = std::move(ProviderFn);
    Running.store(true);
    ServerThread = std::thread(&EnhancedPrometheusExporter::ServerLoop, this, Port);
    return true;
}

void EnhancedPrometheusExporter::Stop()
{
    if (!Running.exchange(false)) return;
    if (ServerThread.joinable()) ServerThread.join();
}

void EnhancedPrometheusExporter::ServerLoop(uint16_t Port)
{
    using namespace Helianthus::Network;
    using namespace Helianthus::Network::Sockets;
    TcpSocket Server;
    NetworkAddress Bind{"0.0.0.0", Port};
    if (Server.Bind(Bind) != NetworkError::NONE) { 
        std::cout << "Bind failed" << std::endl;
        Running.store(false); 
        return; 
    }
    if (Server.Listen(64) != NetworkError::NONE) { 
        std::cout << "Listen failed" << std::endl;
        Running.store(false); 
        return; 
    }
    std::cout << "Enhanced Prometheus Exporter started successfully on port " << Port << std::endl;

    while (Running.load())
    {
        TcpSocket Client;
        auto Err = Server.AcceptClient(Client);
        if (Err != NetworkError::NONE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // 读取请求
        std::string Req;
        char Buffer[2048]; size_t Recv = 0;
        auto ErrRecv = Client.Receive(Buffer, sizeof(Buffer), Recv);
        if (ErrRecv == Helianthus::Network::NetworkError::NONE && Recv > 0)
        {
            Req.assign(Buffer, Buffer + Recv);
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else
        {
            continue;
        }
        
        // 解析请求
        const char* MethodC = nullptr; 
        size_t MethodLen = 0;
        const char* PathC = nullptr; 
        size_t PathLen = 0; 
        int Minor = 1;
        
        struct phr_header Headers[16];
        size_t NumHeaders = 16;
        int ParsedBytes = phr_parse_request(Req.data(), Req.size(), &MethodC, &MethodLen,
                                            &PathC, &PathLen, &Minor, Headers, &NumHeaders, 0);
        
        std::string Method, Path;
        bool Parsed = ParsedBytes >= 0;
        
        if (Parsed)
        {
            Method.assign(MethodC, MethodLen);
            Path.assign(PathC, PathLen);
        }
        else
        {
            // 简单解析作为后备
            size_t FirstSpace = Req.find(' ');
            if (FirstSpace != std::string::npos)
            {
                Method = Req.substr(0, FirstSpace);
                size_t SecondSpace = Req.find(' ', FirstSpace + 1);
                if (SecondSpace != std::string::npos)
                {
                    Path = Req.substr(FirstSpace + 1, SecondSpace - FirstSpace - 1);
                    Parsed = true;
                }
            }
        }

        std::string Resp;
        if (!Parsed)
        {
            const char* Msg = "Bad Request";
            Resp = std::string("HTTP/1.1 400 Bad Request\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
        }
        else if (Path == "/metrics")
        {
            if (Method != "GET" && Method != "HEAD")
            {
                const char* Msg = "Method Not Allowed";
                Resp = std::string("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD\r\nContent-Length: ") +
                       std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
            }
            else
            {
                std::string Body = Provider ? Provider() : std::string();
                if (Body.empty()) Body = "# HELP helianthus_up 1 if exporter is up\n# TYPE helianthus_up gauge\nhelianthus_up 1\n";
                
                // 添加增强指标
                Body += ExportAllEnhancedMetrics();
                
                Resp = std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: ") +
                       std::to_string(Body.size()) + "\r\nConnection: close\r\n\r\n" + (Method == "HEAD" ? std::string() : Body);
            }
        }
        else if (Path == "/health")
        {
            const char* Msg = "ok";
            Resp = std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + (Method == "HEAD" ? "" : Msg);
        }
        else
        {
            const char* Msg = "Not Found";
            Resp = std::string("HTTP/1.1 404 Not Found\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
        }

        size_t Sent = 0;
        Client.Send(Resp.data(), Resp.size(), Sent);
        Client.Disconnect();
    }
}

void EnhancedPrometheusExporter::UpdateBatchPerformance(const std::string& QueueName, uint64_t DurationNs, uint64_t MessageCount)
{
    std::lock_guard<std::mutex> Lock(BatchStatsMutex);
    BatchStatsByQueue[QueueName].AddSample(DurationNs, MessageCount);
}

void EnhancedPrometheusExporter::UpdateZeroCopyPerformance(uint64_t DurationNs)
{
    std::lock_guard<std::mutex> Lock(ZeroCopyStatsMutex);
    ZeroCopyStats.AddSample(DurationNs);
}

void EnhancedPrometheusExporter::UpdateTransactionPerformance(bool Committed, bool RolledBack, bool Timeout, bool Failed, 
                                                            uint64_t CommitTimeNs, uint64_t RollbackTimeNs)
{
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    TransactionStats.UpdateTransactionCount(Committed, RolledBack, Timeout, Failed);
    if (Committed && CommitTimeNs > 0)
    {
        TransactionStats.AddCommitSample(CommitTimeNs);
    }
    if (RolledBack && RollbackTimeNs > 0)
    {
        TransactionStats.AddRollbackSample(RollbackTimeNs);
    }
}

const BatchPerformanceStats& EnhancedPrometheusExporter::GetBatchStats(const std::string& QueueName) const
{
    std::lock_guard<std::mutex> Lock(BatchStatsMutex);
    auto It = BatchStatsByQueue.find(QueueName);
    if (It != BatchStatsByQueue.end())
    {
        return It->second;
    }
    static const BatchPerformanceStats EmptyStats{};
    return EmptyStats;
}

const ZeroCopyPerformanceStats& EnhancedPrometheusExporter::GetZeroCopyStats() const
{
    std::lock_guard<std::mutex> Lock(ZeroCopyStatsMutex);
    return ZeroCopyStats;
}

const TransactionPerformanceStats& EnhancedPrometheusExporter::GetTransactionStats() const
{
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    return TransactionStats;
}

std::string EnhancedPrometheusExporter::FormatMetric(const std::string& Name, double Value, 
                                                   const std::map<std::string, std::string>& Labels)
{
    std::ostringstream Oss;
    Oss << Name;
    
    if (!Labels.empty()) {
        Oss << "{";
        bool First = true;
        for (const auto& Label : Labels) {
            if (!First) Oss << ",";
            Oss << Label.first << "=\"" << Label.second << "\"";
            First = false;
        }
        Oss << "}";
    }
    
    Oss << " " << std::fixed << std::setprecision(6) << Value << "\n";
    return Oss.str();
}

std::string EnhancedPrometheusExporter::FormatHistogram(const std::string& Name, 
                                                      const std::vector<std::pair<double, uint64_t>>& Buckets,
                                                      const std::map<std::string, std::string>& Labels)
{
    std::ostringstream Oss;
    
    // 添加 HELP 和 TYPE
    Oss << "# HELP " << Name << " Histogram of " << Name << "\n";
    Oss << "# TYPE " << Name << " histogram\n";
    
    // 添加桶
    for (const auto& Bucket : Buckets)
    {
        std::map<std::string, std::string> BucketLabels = Labels;
        BucketLabels["le"] = std::to_string(Bucket.first);
        Oss << Name << "_bucket{";
        bool First = true;
        for (const auto& Label : BucketLabels) {
            if (!First) Oss << ",";
            Oss << Label.first << "=\"" << Label.second << "\"";
            First = false;
        }
        Oss << "} " << Bucket.second << "\n";
    }
    
    // 添加总和
    uint64_t Total = 0;
    for (const auto& Bucket : Buckets)
    {
        Total += Bucket.second;
    }
    Oss << Name << "_sum{";
    bool First = true;
    for (const auto& Label : Labels) {
        if (!First) Oss << ",";
        Oss << Label.first << "=\"" << Label.second << "\"";
        First = false;
    }
    Oss << "} " << Total << "\n";
    
    // 添加计数
    Oss << Name << "_count{";
    First = true;
    for (const auto& Label : Labels) {
        if (!First) Oss << ",";
        Oss << Label.first << "=\"" << Label.second << "\"";
        First = false;
    }
    Oss << "} " << Total << "\n";
    
    return Oss.str();
}

std::string EnhancedPrometheusExporter::FormatCounter(const std::string& Name, uint64_t Value,
                                                    const std::map<std::string, std::string>& Labels)
{
    std::ostringstream Oss;
    Oss << Name;
    
    if (!Labels.empty()) {
        Oss << "{";
        bool First = true;
        for (const auto& Label : Labels) {
            if (!First) Oss << ",";
            Oss << Label.first << "=\"" << Label.second << "\"";
            First = false;
        }
        Oss << "}";
    }
    
    Oss << " " << Value << "\n";
    return Oss.str();
}

std::string EnhancedPrometheusExporter::ExportBatchMetrics() const
{
    std::ostringstream Oss;
    
    // 添加 HELP 和 TYPE 注释
    Oss << "# HELP helianthus_batch_duration_ms Batch processing duration in milliseconds\n";
    Oss << "# TYPE helianthus_batch_duration_ms histogram\n";
    Oss << "# HELP helianthus_batch_messages_total Total number of messages in batches\n";
    Oss << "# TYPE helianthus_batch_messages_total counter\n";
    Oss << "# HELP helianthus_batch_count_total Total number of batches\n";
    Oss << "# TYPE helianthus_batch_count_total counter\n";
    
    std::lock_guard<std::mutex> Lock(BatchStatsMutex);
    for (const auto& Pair : BatchStatsByQueue)
    {
        const std::string& QueueName = Pair.first;
        const BatchPerformanceStats& Stats = Pair.second;
        
        std::map<std::string, std::string> Labels = {{"queue", QueueName}};
        
        // 导出直方图
        auto Histogram = Stats.GetDurationHistogram();
        if (!Histogram.empty())
        {
            Oss << FormatHistogram("helianthus_batch_duration_ms", Histogram, Labels);
        }
        
        // 导出计数器
        Oss << FormatCounter("helianthus_batch_count_total", Stats.TotalBatches.load(), Labels);
        Oss << FormatCounter("helianthus_batch_messages_total", Stats.TotalMessages.load(), Labels);
        
        // 导出分位数
        Oss << FormatMetric("helianthus_batch_duration_p50_ms", Stats.GetP50DurationMs(), Labels);
        Oss << FormatMetric("helianthus_batch_duration_p95_ms", Stats.GetP95DurationMs(), Labels);
        Oss << FormatMetric("helianthus_batch_duration_p99_ms", Stats.GetP99DurationMs(), Labels);
        Oss << FormatMetric("helianthus_batch_duration_avg_ms", Stats.GetAverageDurationMs(), Labels);
    }
    
    return Oss.str();
}

std::string EnhancedPrometheusExporter::ExportZeroCopyMetrics() const
{
    std::ostringstream Oss;
    
    // 添加 HELP 和 TYPE 注释
    Oss << "# HELP helianthus_zero_copy_duration_ms Zero-copy operation duration in milliseconds\n";
    Oss << "# TYPE helianthus_zero_copy_duration_ms histogram\n";
    Oss << "# HELP helianthus_zero_copy_operations_total Total number of zero-copy operations\n";
    Oss << "# TYPE helianthus_zero_copy_operations_total counter\n";
    
    std::lock_guard<std::mutex> Lock(ZeroCopyStatsMutex);
    
    // 导出直方图
    auto Histogram = ZeroCopyStats.GetDurationHistogram();
    if (!Histogram.empty())
    {
        Oss << FormatHistogram("helianthus_zero_copy_duration_ms", Histogram, {});
    }
    
    // 导出计数器
    Oss << FormatCounter("helianthus_zero_copy_operations_total", ZeroCopyStats.TotalOperations.load(), {});
    
    // 导出分位数
    Oss << FormatMetric("helianthus_zero_copy_duration_p50_ms", ZeroCopyStats.GetP50DurationMs(), {});
    Oss << FormatMetric("helianthus_zero_copy_duration_p95_ms", ZeroCopyStats.GetP95DurationMs(), {});
    Oss << FormatMetric("helianthus_zero_copy_duration_p99_ms", ZeroCopyStats.GetP99DurationMs(), {});
    Oss << FormatMetric("helianthus_zero_copy_duration_avg_ms", ZeroCopyStats.GetAverageDurationMs(), {});
    
    return Oss.str();
}

std::string EnhancedPrometheusExporter::ExportTransactionMetrics() const
{
    std::ostringstream Oss;
    
    // 添加 HELP 和 TYPE 注释
    Oss << "# HELP helianthus_transaction_commit_duration_ms Transaction commit duration in milliseconds\n";
    Oss << "# TYPE helianthus_transaction_commit_duration_ms histogram\n";
    Oss << "# HELP helianthus_transaction_rollback_duration_ms Transaction rollback duration in milliseconds\n";
    Oss << "# TYPE helianthus_transaction_rollback_duration_ms histogram\n";
    Oss << "# HELP helianthus_transaction_total Total number of transactions\n";
    Oss << "# TYPE helianthus_transaction_total counter\n";
    Oss << "# HELP helianthus_transaction_committed_total Total number of committed transactions\n";
    Oss << "# TYPE helianthus_transaction_committed_total counter\n";
    Oss << "# HELP helianthus_transaction_rolled_back_total Total number of rolled back transactions\n";
    Oss << "# TYPE helianthus_transaction_rolled_back_total counter\n";
    Oss << "# HELP helianthus_transaction_timeout_total Total number of timeout transactions\n";
    Oss << "# TYPE helianthus_transaction_timeout_total counter\n";
    Oss << "# HELP helianthus_transaction_failed_total Total number of failed transactions\n";
    Oss << "# TYPE helianthus_transaction_failed_total counter\n";
    
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    
    // 导出提交时间直方图
    auto CommitHistogram = TransactionStats.GetCommitTimeHistogram();
    if (!CommitHistogram.empty())
    {
        Oss << FormatHistogram("helianthus_transaction_commit_duration_ms", CommitHistogram, {});
    }
    
    // 导出回滚时间直方图
    auto RollbackHistogram = TransactionStats.GetRollbackTimeHistogram();
    if (!RollbackHistogram.empty())
    {
        Oss << FormatHistogram("helianthus_transaction_rollback_duration_ms", RollbackHistogram, {});
    }
    
    // 导出计数器
    Oss << FormatCounter("helianthus_transaction_total", TransactionStats.TotalTransactions.load(), {});
    Oss << FormatCounter("helianthus_transaction_committed_total", TransactionStats.CommittedTransactions.load(), {});
    Oss << FormatCounter("helianthus_transaction_rolled_back_total", TransactionStats.RolledBackTransactions.load(), {});
    Oss << FormatCounter("helianthus_transaction_timeout_total", TransactionStats.TimeoutTransactions.load(), {});
    Oss << FormatCounter("helianthus_transaction_failed_total", TransactionStats.FailedTransactions.load(), {});
    
    // 导出分位数
    Oss << FormatMetric("helianthus_transaction_commit_duration_p50_ms", TransactionStats.GetP50CommitTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_commit_duration_p95_ms", TransactionStats.GetP95CommitTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_commit_duration_p99_ms", TransactionStats.GetP99CommitTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_commit_duration_avg_ms", TransactionStats.GetAverageCommitTimeMs(), {});
    
    Oss << FormatMetric("helianthus_transaction_rollback_duration_p50_ms", TransactionStats.GetP50RollbackTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_rollback_duration_p95_ms", TransactionStats.GetP95RollbackTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_rollback_duration_p99_ms", TransactionStats.GetP99RollbackTimeMs(), {});
    Oss << FormatMetric("helianthus_transaction_rollback_duration_avg_ms", TransactionStats.GetAverageRollbackTimeMs(), {});
    
    // 导出比率
    Oss << FormatMetric("helianthus_transaction_success_rate", TransactionStats.GetSuccessRate(), {});
    Oss << FormatMetric("helianthus_transaction_rollback_rate", TransactionStats.GetRollbackRate(), {});
    Oss << FormatMetric("helianthus_transaction_timeout_rate", TransactionStats.GetTimeoutRate(), {});
    Oss << FormatMetric("helianthus_transaction_failure_rate", TransactionStats.GetFailureRate(), {});
    
    return Oss.str();
}

std::string EnhancedPrometheusExporter::ExportAllEnhancedMetrics() const
{
    std::ostringstream Oss;
    
    Oss << "\n# Enhanced Helianthus Metrics\n";
    Oss << ExportBatchMetrics();
    Oss << "\n";
    Oss << ExportZeroCopyMetrics();
    Oss << "\n";
    Oss << ExportTransactionMetrics();
    
    return Oss.str();
}

void EnhancedPrometheusExporter::ResetBatchStats(const std::string& QueueName)
{
    std::lock_guard<std::mutex> Lock(BatchStatsMutex);
    if (QueueName.empty())
    {
        BatchStatsByQueue.clear();
    }
    else
    {
        BatchStatsByQueue.erase(QueueName);
    }
}

void EnhancedPrometheusExporter::ResetZeroCopyStats()
{
    std::lock_guard<std::mutex> Lock(ZeroCopyStatsMutex);
    ZeroCopyStats.Reset();
}

void EnhancedPrometheusExporter::ResetTransactionStats()
{
    std::lock_guard<std::mutex> Lock(TransactionStatsMutex);
    TransactionStats.Reset();
}

void EnhancedPrometheusExporter::ResetAllStats()
{
    ResetBatchStats();
    ResetZeroCopyStats();
    ResetTransactionStats();
}

} // namespace Helianthus::Monitoring
