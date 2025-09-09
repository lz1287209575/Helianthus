#include "RpcInterceptors.h"
#include "Shared/Common/LogCategories.h"
#include <algorithm>

namespace Helianthus::RPC
{

// LoggingInterceptor implementation
LoggingInterceptor::LoggingInterceptor(bool LogRequests, bool LogResponses, bool LogPerformance)
    : LogRequests(LogRequests), LogResponses(LogResponses), LogPerformance(LogPerformance)
{
}

bool LoggingInterceptor::OnBeforeCall(RpcContext& Context, const RpcMessage& Message)
{
    if (LogRequests)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "RPC Request: Method={}, Id={}, ClientId={}", 
                    Context.MethodName, Context.CallId, Context.ClientId);
    }

    if (LogPerformance)
    {
        std::lock_guard<std::mutex> Lock(StartTimesMutex);
        StartTimes[Context.CallId] = std::chrono::steady_clock::now();
    }

    return true;
}

void LoggingInterceptor::OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result)
{
    if (LogResponses)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "RPC Response: Method={}, Id={}, Success={}, Duration={}ms", 
                    Context.MethodName, Context.CallId, (Result == RpcResult::SUCCESS),
                    LogPerformance ? [&]() {
                        std::lock_guard<std::mutex> Lock(StartTimesMutex);
                        auto It = StartTimes.find(Context.CallId);
                        if (It != StartTimes.end())
                        {
                            auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - It->second).count();
                            StartTimes.erase(It);
                            return Duration;
                        }
                        return 0L;
                    }() : 0);
    }
}

void LoggingInterceptor::OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "RPC Error: Method={}, Id={}, Error={}", 
                 Context.MethodName, Context.CallId, Error);
}

// PerformanceInterceptor implementation
PerformanceInterceptor::PerformanceInterceptor()
{
    ResetStats();
}

bool PerformanceInterceptor::OnBeforeCall(RpcContext& Context, const RpcMessage& Message)
{
    std::lock_guard<std::mutex> Lock(StartTimesMutex);
    StartTimes[Context.CallId] = std::chrono::steady_clock::now();
    return true;
}

void PerformanceInterceptor::OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result)
{
    std::lock_guard<std::mutex> Lock(StartTimesMutex);
    auto It = StartTimes.find(Context.CallId);
    if (It != StartTimes.end())
    {
        auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - It->second).count();
        
        Stats.TotalCalls++;
        if (Result == RpcResult::SUCCESS)
        {
            Stats.SuccessfulCalls++;
        }
        else
        {
            Stats.FailedCalls++;
        }

        Stats.TotalResponseTimeMs += Duration;
        
        uint64_t CurrentMin = Stats.MinResponseTimeMs.load();
        while (Duration < CurrentMin && 
               !Stats.MinResponseTimeMs.compare_exchange_weak(CurrentMin, Duration))
        {
            // Retry
        }

        uint64_t CurrentMax = Stats.MaxResponseTimeMs.load();
        while (Duration > CurrentMax && 
               !Stats.MaxResponseTimeMs.compare_exchange_weak(CurrentMax, Duration))
        {
            // Retry
        }

        StartTimes.erase(It);
    }
}

void PerformanceInterceptor::OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error)
{
    Stats.TotalCalls++;
    Stats.FailedCalls++;
}

void PerformanceInterceptor::GetStats(PerformanceStats& OutStats) const
{
    OutStats.TotalCalls = Stats.TotalCalls.load();
    OutStats.SuccessfulCalls = Stats.SuccessfulCalls.load();
    OutStats.FailedCalls = Stats.FailedCalls.load();
    OutStats.TotalResponseTimeMs = Stats.TotalResponseTimeMs.load();
    OutStats.MinResponseTimeMs = Stats.MinResponseTimeMs.load();
    OutStats.MaxResponseTimeMs = Stats.MaxResponseTimeMs.load();
    OutStats.StartTime = Stats.StartTime;
}

void PerformanceInterceptor::ResetStats()
{
    Stats.TotalCalls = 0;
    Stats.SuccessfulCalls = 0;
    Stats.FailedCalls = 0;
    Stats.TotalResponseTimeMs = 0;
    Stats.MinResponseTimeMs = UINT64_MAX;
    Stats.MaxResponseTimeMs = 0;
    Stats.StartTime = std::chrono::steady_clock::now();
}

// AuthenticationInterceptor implementation
AuthenticationInterceptor::AuthenticationInterceptor(AuthCallback Callback)
    : AuthCallbackFunc(std::move(Callback))
{
}

bool AuthenticationInterceptor::OnBeforeCall(RpcContext& Context, const RpcMessage& Message)
{
    if (!AuthCallbackFunc)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "No authentication callback set");
        return false;
    }

    try
    {
        return AuthCallbackFunc(Context, Message);
    }
    catch (const std::exception& Ex)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "Authentication callback threw exception: {}", Ex.what());
        return false;
    }
}

void AuthenticationInterceptor::OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result)
{
    // No action needed after call
}

void AuthenticationInterceptor::OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "Authentication failed for RPC call: Method={}, Error={}", 
                 Context.MethodName, Error);
}

void AuthenticationInterceptor::SetAuthCallback(AuthCallback Callback)
{
    AuthCallbackFunc = std::move(Callback);
}

// RateLimitInterceptor implementation
RateLimitInterceptor::RateLimitInterceptor(uint32_t MaxRequestsPerSecond, uint32_t BurstSize)
    : MaxRequestsPerSecond(MaxRequestsPerSecond), BurstSize(BurstSize)
    , LastResetTime(std::chrono::steady_clock::now())
{
}

bool RateLimitInterceptor::OnBeforeCall(RpcContext& Context, const RpcMessage& Message)
{
    if (IsRateLimitExceeded())
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "Rate limit exceeded for RPC call: Method={}", Context.MethodName);
        return false;
    }

    CurrentRequests++;
    return true;
}

void RateLimitInterceptor::OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result)
{
    // No action needed after call
}

void RateLimitInterceptor::OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error)
{
    // No action needed on error
}

void RateLimitInterceptor::UpdateRateLimit(uint32_t MaxRequestsPerSecond, uint32_t BurstSize)
{
    std::lock_guard<std::mutex> Lock(RateLimitMutex);
    this->MaxRequestsPerSecond = MaxRequestsPerSecond;
    this->BurstSize = BurstSize;
}

bool RateLimitInterceptor::IsRateLimitExceeded()
{
    std::lock_guard<std::mutex> Lock(RateLimitMutex);
    
    auto Now = std::chrono::steady_clock::now();
    auto Elapsed = std::chrono::duration_cast<std::chrono::seconds>(Now - LastResetTime).count();
    
    if (Elapsed >= 1)
    {
        ResetCounter();
        return false;
    }
    
    uint32_t MaxAllowed = MaxRequestsPerSecond;
    if (BurstSize > 0)
    {
        MaxAllowed = std::min(MaxRequestsPerSecond, BurstSize);
    }
    
    return CurrentRequests.load() >= MaxAllowed;
}

void RateLimitInterceptor::ResetCounter()
{
    CurrentRequests = 0;
    LastResetTime = std::chrono::steady_clock::now();
}

// CacheInterceptor implementation
CacheInterceptor::CacheInterceptor(uint32_t TtlSeconds, CacheKeyGenerator KeyGen)
    : TtlSeconds(TtlSeconds), KeyGenerator(std::move(KeyGen))
{
}

bool CacheInterceptor::OnBeforeCall(RpcContext& Context, const RpcMessage& Message)
{
    std::lock_guard<std::mutex> Lock(CacheMutex);
    
    std::string Key = GenerateCacheKey(Message);
    auto It = Cache.find(Key);
    
    if (It != Cache.end() && IsCacheValid(It->second))
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Cache hit for RPC call: Method={}, Key={}", Context.MethodName, Key);
        // Note: In a real implementation, we would need to modify the context to return cached result
        // For now, we just return true to continue with the call
        return true;
    }
    
    if (It != Cache.end())
    {
        Cache.erase(It);
    }
    
    return true;
}

void CacheInterceptor::OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result)
{
    if (Result != RpcResult::SUCCESS)
    {
        return; // Don't cache error results
    }
    
    std::lock_guard<std::mutex> Lock(CacheMutex);
    
    std::string Key = GenerateCacheKey(Message);
    Cache[Key] = std::make_pair(Result, std::chrono::steady_clock::now());
    
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Cached RPC result: Method={}, Key={}", Context.MethodName, Key);
    
    // Cleanup expired entries periodically
    if (Cache.size() % 100 == 0)
    {
        CleanupExpiredEntries();
    }
}

void CacheInterceptor::OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error)
{
    // Don't cache error results
}

void CacheInterceptor::SetTtl(uint32_t TtlSeconds)
{
    this->TtlSeconds = TtlSeconds;
}

void CacheInterceptor::SetKeyGenerator(CacheKeyGenerator KeyGen)
{
    KeyGenerator = std::move(KeyGen);
}

void CacheInterceptor::ClearCache()
{
    std::lock_guard<std::mutex> Lock(CacheMutex);
    Cache.clear();
}

size_t CacheInterceptor::GetCacheSize() const
{
    std::lock_guard<std::mutex> Lock(CacheMutex);
    return Cache.size();
}

std::string CacheInterceptor::GenerateCacheKey(const RpcMessage& Message)
{
    if (KeyGenerator)
    {
        return KeyGenerator(Message);
    }
    
    // Default key generation: method + parameters hash
    return Message.GetContext().MethodName + "_" + std::to_string(std::hash<std::string>{}(Message.GetParameters()));
}

bool CacheInterceptor::IsCacheValid(const CacheValue& Value)
{
    auto Now = std::chrono::steady_clock::now();
    auto Elapsed = std::chrono::duration_cast<std::chrono::seconds>(Now - Value.second).count();
    return Elapsed < TtlSeconds;
}

void CacheInterceptor::CleanupExpiredEntries()
{
    auto It = Cache.begin();
    while (It != Cache.end())
    {
        if (!IsCacheValid(It->second))
        {
            It = Cache.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

} // namespace Helianthus::RPC
