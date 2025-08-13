#pragma once

#include "HelianthusConfig.h"
#include <cstdint>
#include <string>
#include <memory>
#include <chrono>

namespace Helianthus::Common
{
    // Basic type aliases
    using Timestamp = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::high_resolution_clock::duration;
    using TimestampMs = uint64_t;
    using PlayerId = uint64_t;
    using SessionId = uint64_t;
    using ServerId = uint32_t;

    // Constants
    static constexpr PlayerId InvalidPlayerId = 0;
    static constexpr SessionId InvalidSessionId = 0;
    static constexpr ServerId InvalidServerId = 0;

    // Result codes
    enum class ReusltCode : int32_t
    {
        SUCCESS = 0,
        FAILED = -1,
        INVALID_PARAMETER = -2,
        OUT_OF_MEMORY = -3,
        TIMEOUT = -4,
        NOT_INITIALIZED = -5,
        ALREADY_INITIALIZED = -6,
        NOT_FOUND = -7,
        ALREADY_EXISTS = -8,
        PERMISSION_DENIED = -9,
        INVALID_STATE = -10
    };

    // Log levels
    enum class LogLevel : uint8_t
    {
        DEBUG = HELIANTHUS_LOG_LEVEL_DEBUG,
        INFO = HELIANTHUS_LOG_LEVEL_INFO,
        WARN = HELIANTHUS_LOG_LEVEL_WARN,
        ERROR = HELIANTHUS_LOG_LEVEL_ERROR
    };

    // Thread pool configuration
    struct ThreadPoolConfig
    {
        uint32_t ThreadCount = HELIANTHUS_DEFAULT_THREAD_POOL_SIZE;
        uint32_t QueueSize = 1000;
        bool AutoResize = true;
        uint32_t MaxThreads = 32;
        uint32_t MinThreads = 2;
    };

    // Memory pool configuration
    struct MemoryPoolConfig
    {
        size_t PoolSize = HELIANTHUS_DEFAULT_MEMORY_POOL_SIZE;
        size_t BlockSize = 4096;
        bool AutoExpand = true;
        size_t MaxPoolSize = 512 * 1024 * 1024; // 512MB
    };

    // Service information
    struct ServiceInfo
    {
        ServerId ServerIdValue = InvalidServerId;
        std::string ServiceName;
        std::string ServiceVersion;
        std::string HostAddress;
        uint16_t Port = 0;
        bool IsHealthy = false;
        TimestampMs LastHeartbeat = 0;
        
        ServiceInfo() = default;
        ServiceInfo(ServerId InServerId, const std::string& InServiceName, const std::string& InHostAddress, uint16_t InPort)
            : ServerIdValue(InServerId), ServiceName(InServiceName), HostAddress(InHostAddress), Port(InPort) {}
    };

} // namespace Helianthus::Common
