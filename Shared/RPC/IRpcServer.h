#pragma once

#include "Shared/Network/NetworkTypes.h"

#include <unordered_map>

#include "RpcMessage.h"
#include "RpcTypes.h"

namespace Helianthus::RPC
{
/**
 * @brief RPC service interface
 *
 * Base class for implementing RPC services
 */
class IRpcService
{
public:
    virtual ~IRpcService() = default;

    // Service identification
    virtual std::string GetServiceName() const = 0;
    virtual std::string GetServiceVersion() const = 0;
    virtual std::vector<std::string> GetMethodNames() const = 0;

    // Method invocation
    virtual RpcResult HandleCall(const RpcContext& Context,
                                 const std::string& MethodName,
                                 const std::string& Parameters,
                                 std::string& Result) = 0;

    // Asynchronous method handling
    virtual RpcResult HandleCallAsync(const RpcContext& Context,
                                      const std::string& MethodName,
                                      const std::string& Parameters,
                                      RpcCallback Callback) = 0;

    // Service lifecycle
    virtual RpcResult Initialize()
    {
        return RpcResult::SUCCESS;
    }
    virtual void Shutdown() {}

    // Health check
    virtual bool IsHealthy() const
    {
        return true;
    }
    virtual std::string GetHealthStatus() const
    {
        return "OK";
    }
};

/**
 * @brief Abstract RPC server interface
 */
class IRpcServer
{
public:
    virtual ~IRpcServer() = default;

    // Server lifecycle
    virtual RpcResult Start(const Network::NetworkAddress& BindAddress) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // Configuration
    virtual void SetConfig(const RpcConfig& Config) = 0;
    virtual RpcConfig GetConfig() const = 0;

    // Service management
    virtual RpcResult RegisterService(RpcServicePtr Service) = 0;
    virtual RpcResult UnregisterService(const std::string& ServiceName) = 0;
    virtual std::vector<std::string> GetRegisteredServices() const = 0;
    virtual RpcServicePtr GetService(const std::string& ServiceName) const = 0;

    // Client management
    virtual std::vector<std::string> GetConnectedClients() const = 0;
    virtual void DisconnectClient(const std::string& ClientId) = 0;
    virtual void DisconnectAllClients() = 0;

    // Statistics and monitoring
    virtual RpcStats GetStats() const = 0;
    virtual void ResetStats() = 0;
    virtual std::unordered_map<std::string, RpcStats> GetServiceStats() const = 0;

    // Event handlers
    virtual void SetClientConnectedHandler(std::function<void(const std::string&)> Handler) = 0;
    virtual void SetClientDisconnectedHandler(std::function<void(const std::string&)> Handler) = 0;
    virtual void SetErrorHandler(RpcErrorHandler Handler) = 0;

    // Middleware support
    virtual void AddMiddleware(std::function<bool(RpcContext&)> Middleware) = 0;
    virtual void ClearMiddleware() = 0;
};

/**
 * @brief High-performance RPC server implementation
 */
class RpcServer : public IRpcServer
{
public:
    RpcServer();
    explicit RpcServer(const RpcConfig& Config);
    ~RpcServer();

    // IRpcServer implementation
    RpcResult Start(const Network::NetworkAddress& BindAddress) override;
    void Stop() override;
    bool IsRunning() const override;

    void SetConfig(const RpcConfig& Config) override;
    RpcConfig GetConfig() const override;

    RpcResult RegisterService(RpcServicePtr Service) override;
    RpcResult UnregisterService(const std::string& ServiceName) override;
    std::vector<std::string> GetRegisteredServices() const override;
    RpcServicePtr GetService(const std::string& ServiceName) const override;

    std::vector<std::string> GetConnectedClients() const override;
    void DisconnectClient(const std::string& ClientId) override;
    void DisconnectAllClients() override;

    RpcStats GetStats() const override;
    void ResetStats() override;
    std::unordered_map<std::string, RpcStats> GetServiceStats() const override;

    void SetClientConnectedHandler(std::function<void(const std::string&)> Handler) override;
    void SetClientDisconnectedHandler(std::function<void(const std::string&)> Handler) override;
    void SetErrorHandler(RpcErrorHandler Handler) override;

    void AddMiddleware(std::function<bool(RpcContext&)> Middleware) override;
    void ClearMiddleware() override;

private:
    struct Impl;
    std::unique_ptr<Impl> ImplPtr;

    // Internal methods
    void HandleIncomingMessage(const std::string& ClientId, const RpcMessage& Message);
    void SendResponse(const std::string& ClientId, const RpcMessage& Response);
    RpcResult
    ProcessRequest(const RpcContext& Context, const std::string& Parameters, std::string& Result);
    bool ApplyMiddleware(RpcContext& Context);
    void UpdateStats(const RpcContext& Context, RpcResult Result, uint64_t LatencyMs);
};

/**
 * @brief Base implementation for RPC services
 */
class RpcServiceBase : public IRpcService
{
public:
    explicit RpcServiceBase(const std::string& ServiceName);
    ~RpcServiceBase() override = default;

    // IRpcService implementation
    std::string GetServiceName() const override
    {
        return ServiceName;
    }
    std::string GetServiceVersion() const override
    {
        return ServiceVersion;
    }
    std::vector<std::string> GetMethodNames() const override;

    RpcResult HandleCall(const RpcContext& Context,
                         const std::string& MethodName,
                         const std::string& Parameters,
                         std::string& Result) override;

    RpcResult HandleCallAsync(const RpcContext& Context,
                              const std::string& MethodName,
                              const std::string& Parameters,
                              RpcCallback Callback) override;

protected:
    // Method registration
    void RegisterMethod(const std::string& MethodName, RpcMethodHandler Handler);
    void RegisterAsyncMethod(
        const std::string& MethodName,
        std::function<void(const RpcContext&, const std::string&, RpcCallback)> Handler);

    // Utility methods
    void SetServiceVersion(const std::string& Version)
    {
        ServiceVersion = Version;
    }

    // Template helpers for type-safe method registration
    template <typename TRequest, typename TResponse>
    void RegisterTypedMethod(const std::string& MethodName,
                             std::function<RpcResult(const TRequest&, TResponse&)> Handler);

    template <typename TRequest, typename TResponse>
    void RegisterTypedAsyncMethod(
        const std::string& MethodName,
        std::function<void(const TRequest&, std::function<void(RpcResult, const TResponse&)>)>
            Handler);

private:
    std::string ServiceName;
    std::string ServiceVersion = "1.0.0";
    std::unordered_map<std::string, RpcMethodHandler> Methods;
    std::unordered_map<std::string,
                       std::function<void(const RpcContext&, const std::string&, RpcCallback)>>
        AsyncMethods;
};

// Template implementations
template <typename TRequest, typename TResponse>
void RpcServiceBase::RegisterTypedMethod(
    const std::string& MethodName, std::function<RpcResult(const TRequest&, TResponse&)> Handler)
{
    auto Wrapper = [Handler](const std::string& Parameters) -> std::string
    {
        TRequest Request;  // TODO: Deserialize Parameters to Request
        TResponse Response;

        RpcResult Result = Handler(Request, Response);
        if (Result == RpcResult::SUCCESS)
        {
            // TODO: Serialize Response to string
            return "{}";  // Placeholder
        }
        return "";
    };

    RegisterMethod(MethodName, Wrapper);
}

template <typename TRequest, typename TResponse>
void RpcServiceBase::RegisterTypedAsyncMethod(
    const std::string& MethodName,
    std::function<void(const TRequest&, std::function<void(RpcResult, const TResponse&)>)> Handler)
{
    auto Wrapper =
        [Handler](const RpcContext& Context, const std::string& Parameters, RpcCallback Callback)
    {
        TRequest Request;  // TODO: Deserialize Parameters to Request

        auto TypedCallback = [Callback](RpcResult Result, const TResponse& Response)
        {
            if (Result == RpcResult::SUCCESS)
            {
                // TODO: Serialize Response to string
                Callback(Result, "{}");  // Placeholder
            }
            else
            {
                Callback(Result, "");
            }
        };

        Handler(Request, TypedCallback);
    };

    RegisterAsyncMethod(MethodName, Wrapper);
}

}  // namespace Helianthus::RPC