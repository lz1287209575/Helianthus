#pragma once

#include "Shared/Common/Types.h"
#include "Shared/Message/MessageTypes.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <future>
#include <chrono>

namespace Helianthus::RPC
{
    // RPC result codes
    enum class RpcResult : int32_t
    {
        SUCCESS = 0,
        FAILED = -1,
        TIMEOUT = -2,
        SERVICE_NOT_FOUND = -3,
        METHOD_NOT_FOUND = -4,
        INVALID_PARAMETERS = -5,
        SERIALIZATION_ERROR = -6,
        NETWORK_ERROR = -7,
        SERVER_OVERLOADED = -8,
        CLIENT_ERROR = -9,
        INTERNAL_ERROR = -10
    };

    // RPC call types
    enum class RpcCallType : uint8_t
    {
        REQUEST = 0,        // Regular request-response
        RESPONSE = 1,       // Response to a request
        NOTIFICATION = 2,   // One-way call, no response expected
        HEARTBEAT = 3,      // Keep-alive message
        ERROR = 4          // Error response
    };

    // RPC serialization formats
    enum class SerializationFormat : uint8_t
    {
        JSON = 0,
        BINARY = 1,
        PROTOBUF = 2,
        MSGPACK = 3
    };

    // Type aliases
    using RpcId = uint64_t;
    using ServiceId = uint64_t;
    using MethodId = uint32_t;

    static constexpr RpcId InvalidRpcId = 0;
    static constexpr ServiceId InvalidServiceId = 0;
    static constexpr MethodId InvalidMethodId = 0;

    // RPC call context
    struct RpcContext
    {
        RpcId CallId = InvalidRpcId;
        ServiceId ServiceIdValue = InvalidServiceId;
        MethodId MethodIdValue = InvalidMethodId;
        std::string ServiceName;
        std::string MethodName;
        RpcCallType CallType = RpcCallType::REQUEST;
        SerializationFormat Format = SerializationFormat::JSON;
        Common::TimestampMs Timestamp = 0;
        uint32_t TimeoutMs = 5000;
        uint32_t RetryCount = 0;
        uint32_t MaxRetries = 3;
        std::string ClientId;
        std::string ServerId;
    };

    // RPC configuration
    struct RpcConfig
    {
        uint32_t DefaultTimeoutMs = 5000;
        uint32_t MaxRetries = 3;
        uint32_t MaxConcurrentCalls = 1000;
        uint32_t CallHistorySize = 10000;
        SerializationFormat DefaultFormat = SerializationFormat::JSON;
        bool EnableCompression = false;
        bool EnableEncryption = false;
        bool EnableMetrics = true;
        uint32_t HeartbeatIntervalMs = 30000;
        uint32_t ConnectionPoolSize = 10;
    };

    // RPC statistics
    struct RpcStats
    {
        uint64_t TotalCalls = 0;
        uint64_t SuccessfulCalls = 0;
        uint64_t FailedCalls = 0;
        uint64_t TimeoutCalls = 0;
        uint64_t AverageLatencyMs = 0;
        uint64_t MaxLatencyMs = 0;
        uint64_t MinLatencyMs = 0;
        uint32_t ActiveCalls = 0;
        uint64_t TotalBytesSerialize = 0;
        uint64_t TotalBytesDeserialize = 0;
    };

    // Forward declarations
    class IRpcService;
    class IRpcClient;
    class IRpcServer;
    class RpcCall;
    class RpcResponse;

    // Smart pointer type definitions
    using RpcServicePtr = std::shared_ptr<IRpcService>;
    using RpcClientPtr = std::shared_ptr<IRpcClient>;
    using RpcServerPtr = std::shared_ptr<IRpcServer>;
    using RpcCallPtr = std::shared_ptr<RpcCall>;
    using RpcResponsePtr = std::shared_ptr<RpcResponse>;

    // Callback function types
    using RpcCallback = std::function<void(RpcResult, const std::string&)>;
    using RpcMethodHandler = std::function<std::string(const std::string&)>;
    using RpcServiceHandler = std::function<void(const RpcContext&, const std::string&, RpcCallback)>;
    using RpcErrorHandler = std::function<void(RpcResult, const std::string&)>;

    // Future-based async support
    template<typename T>
    using RpcFuture = std::future<T>;

    template<typename T>
    using RpcPromise = std::promise<T>;

} // namespace Helianthus::RPC