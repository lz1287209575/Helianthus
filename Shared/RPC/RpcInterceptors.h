#pragma once

#include "IRpcInterceptor.h"
#include "RpcTypes.h"

#include <chrono>
#include <unordered_map>
#include <atomic>

namespace Helianthus::RPC
{

/**
 * @brief 日志拦截器
 * 
 * 记录RPC调用的详细信息，包括请求、响应和性能指标
 */
class LoggingInterceptor : public IRpcInterceptor
{
public:
    LoggingInterceptor(bool LogRequests = true, bool LogResponses = true, bool LogPerformance = true);

    bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override;
    void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override;
    void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override;
    std::string GetName() const override { return "LoggingInterceptor"; }
    int GetPriority() const override { return 100; }

private:
    bool LogRequests;
    bool LogResponses;
    bool LogPerformance;
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> StartTimes;
    mutable std::mutex StartTimesMutex;
};

/**
 * @brief 性能监控拦截器
 * 
 * 收集RPC调用的性能指标，如响应时间、吞吐量等
 */
class PerformanceInterceptor : public IRpcInterceptor
{
public:
    PerformanceInterceptor();

    bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override;
    void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override;
    void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override;
    std::string GetName() const override { return "PerformanceInterceptor"; }
    int GetPriority() const override { return 200; }

    // 性能统计接口
    struct PerformanceStats
    {
        std::atomic<uint64_t> TotalCalls{0};
        std::atomic<uint64_t> SuccessfulCalls{0};
        std::atomic<uint64_t> FailedCalls{0};
        std::atomic<uint64_t> TotalResponseTimeMs{0};
        std::atomic<uint64_t> MinResponseTimeMs{UINT64_MAX};
        std::atomic<uint64_t> MaxResponseTimeMs{0};
        std::chrono::steady_clock::time_point StartTime;
        
        PerformanceStats() : StartTime(std::chrono::steady_clock::now()) {}
    };

    void GetStats(PerformanceStats& OutStats) const;
    void ResetStats();

private:
    mutable PerformanceStats Stats;
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> StartTimes;
    mutable std::mutex StartTimesMutex;
};

/**
 * @brief 认证拦截器
 * 
 * 验证RPC调用的身份和权限
 */
class AuthenticationInterceptor : public IRpcInterceptor
{
public:
    using AuthCallback = std::function<bool(const RpcContext&, const RpcMessage&)>;

    AuthenticationInterceptor(AuthCallback Callback);

    bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override;
    void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override;
    void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override;
    std::string GetName() const override { return "AuthenticationInterceptor"; }
    int GetPriority() const override { return 10; }

    void SetAuthCallback(AuthCallback Callback);

private:
    AuthCallback AuthCallbackFunc;
};

/**
 * @brief 限流拦截器
 * 
 * 限制RPC调用的频率，防止系统过载
 */
class RateLimitInterceptor : public IRpcInterceptor
{
public:
    RateLimitInterceptor(uint32_t MaxRequestsPerSecond, uint32_t BurstSize = 0);

    bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override;
    void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override;
    void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override;
    std::string GetName() const override { return "RateLimitInterceptor"; }
    int GetPriority() const override { return 50; }

    void UpdateRateLimit(uint32_t MaxRequestsPerSecond, uint32_t BurstSize = 0);

private:
    uint32_t MaxRequestsPerSecond;
    uint32_t BurstSize;
    std::atomic<uint32_t> CurrentRequests{0};
    std::chrono::steady_clock::time_point LastResetTime;
    mutable std::mutex RateLimitMutex;

    bool IsRateLimitExceeded();
    void ResetCounter();
};

/**
 * @brief 缓存拦截器
 * 
 * 缓存RPC调用结果，提高响应速度
 */
class CacheInterceptor : public IRpcInterceptor
{
public:
    using CacheKeyGenerator = std::function<std::string(const RpcMessage&)>;
    using CacheValue = std::pair<RpcResult, std::chrono::steady_clock::time_point>;

    CacheInterceptor(uint32_t TtlSeconds = 300, CacheKeyGenerator KeyGen = nullptr);

    bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override;
    void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override;
    void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override;
    std::string GetName() const override { return "CacheInterceptor"; }
    int GetPriority() const override { return 300; }

    void SetTtl(uint32_t TtlSeconds);
    void SetKeyGenerator(CacheKeyGenerator KeyGen);
    void ClearCache();
    size_t GetCacheSize() const;

private:
    uint32_t TtlSeconds;
    CacheKeyGenerator KeyGenerator;
    std::unordered_map<std::string, CacheValue> Cache;
    mutable std::mutex CacheMutex;

    std::string GenerateCacheKey(const RpcMessage& Message);
    bool IsCacheValid(const CacheValue& Value);
    void CleanupExpiredEntries();
};

} // namespace Helianthus::RPC
