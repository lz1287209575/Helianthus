#pragma once

#include "RpcTypes.h"
#include "RpcMessage.h"
#include "Shared/Network/NetworkTypes.h"
#include <functional>
#include <memory>

namespace Helianthus::RPC
{
    /**
     * @brief Abstract RPC client interface
     * 
     * Provides high-level RPC client functionality for making remote calls
     */
    class IRpcClient
    {
    public:
        virtual ~IRpcClient() = default;

        // Connection management
        virtual RpcResult Connect(const Network::NetworkAddress& ServerAddress) = 0;
        virtual void Disconnect() = 0;
        virtual bool IsConnected() const = 0;

        // Configuration
        virtual void SetConfig(const RpcConfig& Config) = 0;
        virtual RpcConfig GetConfig() const = 0;

        // Synchronous calls
        virtual RpcResult Call(const std::string& ServiceName, 
                              const std::string& MethodName,
                              const std::string& Parameters,
                              std::string& Result,
                              uint32_t TimeoutMs = 0) = 0;

        // Asynchronous calls
        virtual RpcResult CallAsync(const std::string& ServiceName,
                                   const std::string& MethodName, 
                                   const std::string& Parameters,
                                   RpcCallback Callback,
                                   uint32_t TimeoutMs = 0) = 0;

        // Future-based async calls
        virtual RpcFuture<std::pair<RpcResult, std::string>> CallFuture(
            const std::string& ServiceName,
            const std::string& MethodName,
            const std::string& Parameters,
            uint32_t TimeoutMs = 0) = 0;

        // One-way notifications (no response expected)
        virtual RpcResult Notify(const std::string& ServiceName,
                                const std::string& MethodName,
                                const std::string& Parameters) = 0;

        // Batch calls (multiple calls in one network round-trip)
        virtual RpcResult BatchCall(const std::vector<RpcContext>& Calls,
                                   std::vector<std::pair<RpcResult, std::string>>& Results,
                                   uint32_t TimeoutMs = 0) = 0;

        // Template helpers for type-safe calls
        template<typename TRequest, typename TResponse>
        RpcResult TypedCall(const std::string& ServiceName,
                           const std::string& MethodName,
                           const TRequest& Request,
                           TResponse& Response,
                           uint32_t TimeoutMs = 0);

        template<typename TRequest, typename TResponse>
        RpcResult TypedCallAsync(const std::string& ServiceName,
                                const std::string& MethodName,
                                const TRequest& Request,
                                std::function<void(RpcResult, const TResponse&)> Callback,
                                uint32_t TimeoutMs = 0);

        // Statistics and monitoring
        virtual RpcStats GetStats() const = 0;
        virtual void ResetStats() = 0;

        // Event handlers
        virtual void SetConnectionStateHandler(std::function<void(bool)> Handler) = 0;
        virtual void SetErrorHandler(RpcErrorHandler Handler) = 0;

        // Service discovery integration
        virtual void EnableServiceDiscovery(bool Enable) = 0;
        virtual std::vector<std::string> GetAvailableServices() = 0;
    };

    /**
     * @brief High-performance RPC client implementation
     */
    class RpcClient : public IRpcClient
    {
    public:
        RpcClient();
        explicit RpcClient(const RpcConfig& Config);
        ~RpcClient();

        // IRpcClient implementation
        RpcResult Connect(const Network::NetworkAddress& ServerAddress) override;
        void Disconnect() override;
        bool IsConnected() const override;

        void SetConfig(const RpcConfig& Config) override;
        RpcConfig GetConfig() const override;

        RpcResult Call(const std::string& ServiceName,
                      const std::string& MethodName,
                      const std::string& Parameters,
                      std::string& Result,
                      uint32_t TimeoutMs = 0) override;

        RpcResult CallAsync(const std::string& ServiceName,
                           const std::string& MethodName,
                           const std::string& Parameters,
                           RpcCallback Callback,
                           uint32_t TimeoutMs = 0) override;

        RpcFuture<std::pair<RpcResult, std::string>> CallFuture(
            const std::string& ServiceName,
            const std::string& MethodName,
            const std::string& Parameters,
            uint32_t TimeoutMs = 0) override;

        RpcResult Notify(const std::string& ServiceName,
                        const std::string& MethodName,
                        const std::string& Parameters) override;

        RpcResult BatchCall(const std::vector<RpcContext>& Calls,
                           std::vector<std::pair<RpcResult, std::string>>& Results,
                           uint32_t TimeoutMs = 0) override;

        RpcStats GetStats() const override;
        void ResetStats() override;

        void SetConnectionStateHandler(std::function<void(bool)> Handler) override;
        void SetErrorHandler(RpcErrorHandler Handler) override;

        void EnableServiceDiscovery(bool Enable) override;
        std::vector<std::string> GetAvailableServices() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> ImplPtr;

        // Internal methods
        RpcId GenerateCallId();
        RpcResult SendRequest(const RpcMessage& Request, uint32_t TimeoutMs);
        void HandleResponse(const RpcMessage& Response);
        void ProcessPendingCalls();
        void CleanupExpiredCalls();
    };

    // Template implementations
    template<typename TRequest, typename TResponse>
    RpcResult IRpcClient::TypedCall(const std::string& ServiceName,
                                   const std::string& MethodName,
                                   const TRequest& Request,
                                   TResponse& Response,
                                   uint32_t TimeoutMs)
    {
        // Serialize request (placeholder - would use actual serializer)
        std::string RequestStr = "{}"; // TODO: Implement proper serialization
        std::string ResultStr;
        
        RpcResult Result = Call(ServiceName, MethodName, RequestStr, ResultStr, TimeoutMs);
        if (Result == RpcResult::SUCCESS)
        {
            // Deserialize response (placeholder)
            // TODO: Implement proper deserialization
        }
        
        return Result;
    }

    template<typename TRequest, typename TResponse>
    RpcResult IRpcClient::TypedCallAsync(const std::string& ServiceName,
                                        const std::string& MethodName,
                                        const TRequest& Request,
                                        std::function<void(RpcResult, const TResponse&)> Callback,
                                        uint32_t TimeoutMs)
    {
        // Serialize request (placeholder)
        std::string RequestStr = "{}";
        
        auto WrappedCallback = [Callback](RpcResult Result, const std::string& ResponseStr) {
            if (Result == RpcResult::SUCCESS)
            {
                TResponse Response; // TODO: Deserialize ResponseStr to Response
                Callback(Result, Response);
            }
            else
            {
                TResponse EmptyResponse;
                Callback(Result, EmptyResponse);
            }
        };
        
        return CallAsync(ServiceName, MethodName, RequestStr, WrappedCallback, TimeoutMs);
    }

} // namespace Helianthus::RPC