#pragma once

#include "RpcTypes.h"
#include "RpcSerializer.h"
#include "RpcMessage.h"
#include "IRpcServer.h"
#include "IRpcInterceptor.h"

#include <unordered_map>
#include <memory>
#include <mutex>

namespace Helianthus::RPC
{

/**
 * @brief RPC message handler for processing incoming requests
 */
class RpcMessageHandler
{
public:
    RpcMessageHandler();
    explicit RpcMessageHandler(SerializationFormat DefaultFormat);
    ~RpcMessageHandler() = default;

    // Message processing
    RpcResult ProcessMessage(const RpcMessage& Message, RpcMessage& Response);
    RpcResult ProcessRequest(const RpcContext& Context, const std::string& Parameters, std::string& Result);
    RpcResult ProcessResponse(const RpcMessage& Message);

    // Service registration
    void RegisterService(RpcServicePtr Service);
    void UnregisterService(const std::string& ServiceName);
    RpcServicePtr GetService(const std::string& ServiceName) const;

    // Serialization management
    void SetDefaultFormat(SerializationFormat Format);
    SerializationFormat GetDefaultFormat() const;
    void SetSerializer(SerializationFormat Format, std::unique_ptr<IRpcSerializer> Serializer);
    IRpcSerializer* GetSerializer(SerializationFormat Format) const;

    // Middleware support
    void AddMiddleware(std::function<bool(RpcContext&)> Middleware);
    void ClearMiddleware();
    bool ApplyMiddleware(RpcContext& Context);

    // Interceptor support
    void AddInterceptor(RpcInterceptorPtr Interceptor);
    void RemoveInterceptor(const std::string& Name);
    void ClearInterceptors();
    RpcInterceptorChain& GetInterceptorChain();

    // Statistics
    RpcStats GetStats() const;
    void ResetStats();
    void UpdateStats(const RpcContext& Context, RpcResult Result, uint64_t LatencyMs);

    // Error handling
    void SetErrorHandler(RpcErrorHandler Handler);
    void HandleError(RpcResult Error, const std::string& Message);

private:
    // Service management
    std::unordered_map<std::string, RpcServicePtr> Services;
    mutable std::mutex ServicesMutex;

    // Serialization
    std::unordered_map<SerializationFormat, std::unique_ptr<IRpcSerializer>> Serializers;
    SerializationFormat DefaultFormat;
    mutable std::mutex SerializersMutex;

    // Middleware
    std::vector<std::function<bool(RpcContext&)>> MiddlewareChain;
    mutable std::mutex MiddlewareMutex;

    // Interceptors
    RpcInterceptorChain InterceptorChain;

    // Statistics
    mutable RpcStats Stats;
    mutable std::mutex StatsMutex;

    // Error handling
    RpcErrorHandler ErrorHandler;
    mutable std::mutex ErrorHandlerMutex;

    // Internal methods
    RpcResult ValidateMessage(const RpcMessage& Message) const;
    RpcResult RouteToService(const RpcContext& Context, const std::string& Parameters, std::string& Result);
    std::string SerializeResponse(const RpcContext& Context, const std::string& Result) const;
    bool DeserializeRequest(const std::string& Data, const RpcContext& Context, std::string& Parameters) const;
};

}  // namespace Helianthus::RPC
