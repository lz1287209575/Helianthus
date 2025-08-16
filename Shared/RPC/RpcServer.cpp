#include "IRpcServer.h"
#include "NetworkAdapterSimple.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

namespace Helianthus::RPC
{
    // Internal implementation structure for RpcServer
    struct RpcServer::Impl
    {
        RpcConfig Config;
        std::shared_ptr<NetworkAdapterSimple> NetworkManager;
        Network::NetworkAddress BindAddress;
        
        // Service registry
        std::unordered_map<std::string, RpcServicePtr> Services;
        std::mutex ServicesMutex;
        
        // Client management
        std::unordered_map<std::string, Network::ConnectionId> Clients;
        std::unordered_map<Network::ConnectionId, std::string> ConnectionToClient;
        std::mutex ClientsMutex;
        
        // Statistics
        mutable std::mutex StatsMutex;
        RpcStats GlobalStats;
        std::unordered_map<std::string, RpcStats> ServiceStats;
        
        // Event handlers
        std::function<void(const std::string&)> ClientConnectedHandler;
        std::function<void(const std::string&)> ClientDisconnectedHandler;
        RpcErrorHandler ErrorHandler;
        
        // Middleware
        std::vector<std::function<bool(RpcContext&)>> Middleware;
        std::mutex MiddlewareMutex;
        
        // Worker thread pool
        std::vector<std::thread> WorkerThreads;
        std::atomic<bool> ShouldStop{false};
        
        bool IsRunning = false;
    };

    RpcServer::RpcServer() : RpcServer(RpcConfig{})
    {
    }

    RpcServer::RpcServer(const RpcConfig& Config) : ImplPtr(std::make_unique<Impl>())
    {
        ImplPtr->Config = Config;
        ImplPtr->NetworkManager = std::make_shared<NetworkAdapterSimple>();
    }

    RpcServer::~RpcServer()
    {
        Stop();
    }

    RpcResult RpcServer::Start(const Network::NetworkAddress& BindAddress)
    {
        if (ImplPtr->IsRunning)
        {
            return RpcResult::SUCCESS;
        }
        
        ImplPtr->BindAddress = BindAddress;
        
        // Initialize network manager
        Network::NetworkConfig NetConfig;
        NetConfig.MaxConnections = ImplPtr->Config.MaxConcurrentCalls;
        NetConfig.ConnectionTimeoutMs = ImplPtr->Config.DefaultTimeoutMs;
        
        Network::NetworkError InitResult = ImplPtr->NetworkManager->Initialize(NetConfig);
        if (InitResult != Network::NetworkError::SUCCESS)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Start server
        Network::NetworkError StartResult = ImplPtr->NetworkManager->StartServer(BindAddress, Network::ProtocolType::TCP);
        if (StartResult != Network::NetworkError::SUCCESS)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Set up event handlers
        ImplPtr->NetworkManager->SetOnClientConnectedCallback([this](Network::ConnectionId ConnId) {
            std::string ClientId = "client_" + std::to_string(ConnId);
            
            {
                std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
                ImplPtr->Clients[ClientId] = ConnId;
                ImplPtr->ConnectionToClient[ConnId] = ClientId;
            }
            
            if (ImplPtr->ClientConnectedHandler)
            {
                ImplPtr->ClientConnectedHandler(ClientId);
            }
        });
        
        ImplPtr->NetworkManager->SetOnClientDisconnectedCallback([this](Network::ConnectionId ConnId, Network::NetworkError Error) {
            std::string ClientId;
            
            {
                std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
                auto It = ImplPtr->ConnectionToClient.find(ConnId);
                if (It != ImplPtr->ConnectionToClient.end())
                {
                    ClientId = It->second;
                    ImplPtr->Clients.erase(ClientId);
                    ImplPtr->ConnectionToClient.erase(ConnId);
                }
            }
            
            if (!ClientId.empty() && ImplPtr->ClientDisconnectedHandler)
            {
                ImplPtr->ClientDisconnectedHandler(ClientId);
            }
        });
        
        ImplPtr->NetworkManager->SetOnDataReceivedCallback([this](Network::ConnectionId ConnId, const uint8_t* Data, size_t Size) {
            std::string ClientId;
            
            {
                std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
                auto It = ImplPtr->ConnectionToClient.find(ConnId);
                if (It != ImplPtr->ConnectionToClient.end())
                {
                    ClientId = It->second;
                }
            }
            
            if (!ClientId.empty())
            {
                // Parse received data as RPC message
                std::string DataStr(reinterpret_cast<const char*>(Data), Size);
                auto Message = Message::Message::Create(Message::MessageType::CUSTOM_MESSAGE_START);
                Message->SetPayload(DataStr);
                
                RpcMessage RpcMsg(Message);
                HandleIncomingMessage(ClientId, RpcMsg);
            }
        });
        
        // Start worker threads
        size_t ThreadCount = std::max(1U, ImplPtr->Config.ConnectionPoolSize / 2);
        for (size_t i = 0; i < ThreadCount; ++i)
        {
            ImplPtr->WorkerThreads.emplace_back([this]() {
                while (!ImplPtr->ShouldStop)
                {
                    // Process network events
                    ImplPtr->NetworkManager->ProcessNetworkEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
        
        ImplPtr->IsRunning = true;
        return RpcResult::SUCCESS;
    }

    void RpcServer::Stop()
    {
        if (!ImplPtr->IsRunning)
        {
            return;
        }
        
        ImplPtr->ShouldStop = true;
        
        // Stop worker threads
        for (auto& Thread : ImplPtr->WorkerThreads)
        {
            if (Thread.joinable())
            {
                Thread.join();
            }
        }
        ImplPtr->WorkerThreads.clear();
        
        // Stop network manager
        ImplPtr->NetworkManager->StopServer();
        
        // Clear clients
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
            ImplPtr->Clients.clear();
            ImplPtr->ConnectionToClient.clear();
        }
        
        ImplPtr->IsRunning = false;
    }

    bool RpcServer::IsRunning() const
    {
        return ImplPtr->IsRunning;
    }

    void RpcServer::SetConfig(const RpcConfig& Config)
    {
        ImplPtr->Config = Config;
    }

    RpcConfig RpcServer::GetConfig() const
    {
        return ImplPtr->Config;
    }

    RpcResult RpcServer::RegisterService(RpcServicePtr Service)
    {
        if (!Service)
        {
            return RpcResult::INVALID_PARAMETERS;
        }
        
        std::string ServiceName = Service->GetServiceName();
        if (ServiceName.empty())
        {
            return RpcResult::INVALID_PARAMETERS;
        }
        
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
            ImplPtr->Services[ServiceName] = Service;
        }
        
        // Initialize service
        RpcResult InitResult = Service->Initialize();
        if (InitResult != RpcResult::SUCCESS)
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
            ImplPtr->Services.erase(ServiceName);
            return InitResult;
        }
        
        return RpcResult::SUCCESS;
    }

    RpcResult RpcServer::UnregisterService(const std::string& ServiceName)
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
        
        auto It = ImplPtr->Services.find(ServiceName);
        if (It == ImplPtr->Services.end())
        {
            return RpcResult::SERVICE_NOT_FOUND;
        }
        
        // Shutdown service
        It->second->Shutdown();
        ImplPtr->Services.erase(It);
        
        return RpcResult::SUCCESS;
    }

    std::vector<std::string> RpcServer::GetRegisteredServices() const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
        
        std::vector<std::string> ServiceNames;
        ServiceNames.reserve(ImplPtr->Services.size());
        
        for (const auto& [Name, Service] : ImplPtr->Services)
        {
            ServiceNames.push_back(Name);
        }
        
        return ServiceNames;
    }

    RpcServicePtr RpcServer::GetService(const std::string& ServiceName) const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
        
        auto It = ImplPtr->Services.find(ServiceName);
        if (It != ImplPtr->Services.end())
        {
            return It->second;
        }
        
        return nullptr;
    }

    std::vector<std::string> RpcServer::GetConnectedClients() const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
        
        std::vector<std::string> ClientIds;
        ClientIds.reserve(ImplPtr->Clients.size());
        
        for (const auto& [ClientId, ConnId] : ImplPtr->Clients)
        {
            ClientIds.push_back(ClientId);
        }
        
        return ClientIds;
    }

    void RpcServer::DisconnectClient(const std::string& ClientId)
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
        
        auto It = ImplPtr->Clients.find(ClientId);
        if (It != ImplPtr->Clients.end())
        {
            ImplPtr->NetworkManager->DisconnectClient(It->second);
        }
    }

    void RpcServer::DisconnectAllClients()
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
        
        for (const auto& [ClientId, ConnId] : ImplPtr->Clients)
        {
            ImplPtr->NetworkManager->DisconnectClient(ConnId);
        }
    }

    RpcStats RpcServer::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        return ImplPtr->GlobalStats;
    }

    void RpcServer::ResetStats()
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        ImplPtr->GlobalStats = RpcStats{};
        ImplPtr->ServiceStats.clear();
    }

    std::unordered_map<std::string, RpcStats> RpcServer::GetServiceStats() const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        return ImplPtr->ServiceStats;
    }

    void RpcServer::SetClientConnectedHandler(std::function<void(const std::string&)> Handler)
    {
        ImplPtr->ClientConnectedHandler = Handler;
    }

    void RpcServer::SetClientDisconnectedHandler(std::function<void(const std::string&)> Handler)
    {
        ImplPtr->ClientDisconnectedHandler = Handler;
    }

    void RpcServer::SetErrorHandler(RpcErrorHandler Handler)
    {
        ImplPtr->ErrorHandler = Handler;
    }

    void RpcServer::AddMiddleware(std::function<bool(RpcContext&)> Middleware)
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->MiddlewareMutex);
        ImplPtr->Middleware.push_back(Middleware);
    }

    void RpcServer::ClearMiddleware()
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->MiddlewareMutex);
        ImplPtr->Middleware.clear();
    }

    // Private methods
    void RpcServer::HandleIncomingMessage(const std::string& ClientId, const RpcMessage& Message)
    {
        auto StartTime = std::chrono::steady_clock::now();
        
        RpcContext Context = Message.GetContext();
        Context.ClientId = ClientId;
        
        // Apply middleware
        if (!ApplyMiddleware(Context))
        {
            // Middleware rejected the request
            RpcMessage ErrorResponse(Context);
            ErrorResponse.SetError(RpcResult::CLIENT_ERROR, "Request rejected by middleware");
            SendResponse(ClientId, ErrorResponse);
            return;
        }
        
        // Handle different call types
        if (Message.IsNotification())
        {
            // One-way call, no response needed
            std::string DummyResult;
            ProcessRequest(Context, Message.GetParameters(), DummyResult);
            return;
        }
        
        if (Message.IsRequest())
        {
            std::string Result;
            RpcResult ProcessResult = ProcessRequest(Context, Message.GetParameters(), Result);
            
            // Create response
            RpcContext ResponseContext = Context;
            ResponseContext.CallType = RpcCallType::RESPONSE;
            
            RpcMessage Response(ResponseContext);
            if (ProcessResult == RpcResult::SUCCESS)
            {
                Response.SetResult(Result);
            }
            else
            {
                Response.SetError(ProcessResult, "Service call failed");
            }
            
            SendResponse(ClientId, Response);
            
            // Update statistics
            auto EndTime = std::chrono::steady_clock::now();
            auto Latency = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
            UpdateStats(Context, ProcessResult, Latency.count());
        }
    }

    void RpcServer::SendResponse(const std::string& ClientId, const RpcMessage& Response)
    {
        Network::ConnectionId ConnId;
        
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->ClientsMutex);
            auto It = ImplPtr->Clients.find(ClientId);
            if (It == ImplPtr->Clients.end())
            {
                return; // Client not found
            }
            ConnId = It->second;
        }
        
        auto Message = Response.ToMessage();
        std::string Payload = Message->GetJsonPayload();
        
        ImplPtr->NetworkManager->SendToClient(
            ConnId,
            reinterpret_cast<const uint8_t*>(Payload.data()),
            Payload.size()
        );
    }

    RpcResult RpcServer::ProcessRequest(const RpcContext& Context, const std::string& Parameters, std::string& Result)
    {
        // Find service
        RpcServicePtr Service;
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->ServicesMutex);
            auto It = ImplPtr->Services.find(Context.ServiceName);
            if (It == ImplPtr->Services.end())
            {
                return RpcResult::SERVICE_NOT_FOUND;
            }
            Service = It->second;
        }
        
        // Check service health
        if (!Service->IsHealthy())
        {
            return RpcResult::SERVICE_NOT_FOUND;
        }
        
        // Call service method
        return Service->HandleCall(Context, Context.MethodName, Parameters, Result);
    }

    bool RpcServer::ApplyMiddleware(RpcContext& Context)
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->MiddlewareMutex);
        
        for (const auto& Middleware : ImplPtr->Middleware)
        {
            if (!Middleware(Context))
            {
                return false;
            }
        }
        
        return true;
    }

    void RpcServer::UpdateStats(const RpcContext& Context, RpcResult Result, uint64_t LatencyMs)
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        
        // Update global stats
        ImplPtr->GlobalStats.TotalCalls++;
        if (Result == RpcResult::SUCCESS)
        {
            ImplPtr->GlobalStats.SuccessfulCalls++;
        }
        else if (Result == RpcResult::TIMEOUT)
        {
            ImplPtr->GlobalStats.TimeoutCalls++;
            ImplPtr->GlobalStats.FailedCalls++;
        }
        else
        {
            ImplPtr->GlobalStats.FailedCalls++;
        }
        
        // Update latency stats
        if (ImplPtr->GlobalStats.TotalCalls == 1)
        {
            ImplPtr->GlobalStats.MinLatencyMs = LatencyMs;
            ImplPtr->GlobalStats.MaxLatencyMs = LatencyMs;
            ImplPtr->GlobalStats.AverageLatencyMs = LatencyMs;
        }
        else
        {
            ImplPtr->GlobalStats.MinLatencyMs = std::min(ImplPtr->GlobalStats.MinLatencyMs, LatencyMs);
            ImplPtr->GlobalStats.MaxLatencyMs = std::max(ImplPtr->GlobalStats.MaxLatencyMs, LatencyMs);
            
            // Update running average
            uint64_t TotalLatency = ImplPtr->GlobalStats.AverageLatencyMs * (ImplPtr->GlobalStats.TotalCalls - 1) + LatencyMs;
            ImplPtr->GlobalStats.AverageLatencyMs = TotalLatency / ImplPtr->GlobalStats.TotalCalls;
        }
        
        // Update service-specific stats
        auto& ServiceStats = ImplPtr->ServiceStats[Context.ServiceName];
        ServiceStats.TotalCalls++;
        if (Result == RpcResult::SUCCESS)
        {
            ServiceStats.SuccessfulCalls++;
        }
        else
        {
            ServiceStats.FailedCalls++;
        }
    }

    // RpcServiceBase implementation
    RpcServiceBase::RpcServiceBase(const std::string& ServiceName) : ServiceName(ServiceName)
    {
    }

    std::vector<std::string> RpcServiceBase::GetMethodNames() const
    {
        std::vector<std::string> MethodNames;
        MethodNames.reserve(Methods.size() + AsyncMethods.size());
        
        for (const auto& [Name, Handler] : Methods)
        {
            MethodNames.push_back(Name);
        }
        
        for (const auto& [Name, Handler] : AsyncMethods)
        {
            MethodNames.push_back(Name);
        }
        
        return MethodNames;
    }

    RpcResult RpcServiceBase::HandleCall(const RpcContext& Context,
                                        const std::string& MethodName,
                                        const std::string& Parameters,
                                        std::string& Result)
    {
        auto It = Methods.find(MethodName);
        if (It == Methods.end())
        {
            return RpcResult::METHOD_NOT_FOUND;
        }
        
        try
        {
            Result = It->second(Parameters);
            return RpcResult::SUCCESS;
        }
        catch (const std::exception&)
        {
            return RpcResult::INTERNAL_ERROR;
        }
    }

    RpcResult RpcServiceBase::HandleCallAsync(const RpcContext& Context,
                                             const std::string& MethodName,
                                             const std::string& Parameters,
                                             RpcCallback Callback)
    {
        auto It = AsyncMethods.find(MethodName);
        if (It == AsyncMethods.end())
        {
            return RpcResult::METHOD_NOT_FOUND;
        }
        
        try
        {
            It->second(Context, Parameters, Callback);
            return RpcResult::SUCCESS;
        }
        catch (const std::exception&)
        {
            return RpcResult::INTERNAL_ERROR;
        }
    }

    void RpcServiceBase::RegisterMethod(const std::string& MethodName, RpcMethodHandler Handler)
    {
        Methods[MethodName] = Handler;
    }

    void RpcServiceBase::RegisterAsyncMethod(const std::string& MethodName,
                                           std::function<void(const RpcContext&, const std::string&, RpcCallback)> Handler)
    {
        AsyncMethods[MethodName] = Handler;
    }

} // namespace Helianthus::RPC