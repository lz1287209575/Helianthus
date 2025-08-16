#include "IRpcClient.h"
#include "NetworkAdapterSimple.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>

namespace Helianthus::RPC
{
    // Internal implementation structure
    struct RpcClient::Impl
    {
        RpcConfig Config;
        std::shared_ptr<NetworkAdapterSimple> NetworkManager;
        Network::ConnectionId ConnectionId = Network::InvalidConnectionId;
        Network::NetworkAddress ServerAddress;
        
        // Call tracking
        std::atomic<RpcId> NextCallId{1};
        std::unordered_map<RpcId, RpcPromise<std::pair<RpcResult, std::string>>> PendingCalls;
        std::unordered_map<RpcId, RpcCallback> AsyncCallbacks;
        std::unordered_map<RpcId, std::chrono::steady_clock::time_point> CallTimestamps;
        std::mutex CallsMutex;
        
        // Statistics
        mutable std::mutex StatsMutex;
        RpcStats Stats;
        
        // Event handlers
        std::function<void(bool)> ConnectionStateHandler;
        RpcErrorHandler ErrorHandler;
        
        // Worker thread for processing responses
        std::thread WorkerThread;
        std::atomic<bool> ShouldStop{false};
        
        bool IsConnected = false;
    };

    RpcClient::RpcClient() : RpcClient(RpcConfig{})
    {
    }

    RpcClient::RpcClient(const RpcConfig& Config) : ImplPtr(std::make_unique<Impl>())
    {
        ImplPtr->Config = Config;
        ImplPtr->NetworkManager = std::make_shared<NetworkAdapterSimple>();
        
        // Initialize network manager
        Network::NetworkConfig NetConfig;
        NetConfig.MaxConnections = 1; // Client only needs one connection
        NetConfig.ConnectionTimeoutMs = Config.DefaultTimeoutMs;
        ImplPtr->NetworkManager->Initialize(NetConfig);
        
        // Start worker thread
        ImplPtr->WorkerThread = std::thread([this]() {
            while (!ImplPtr->ShouldStop)
            {
                ProcessPendingCalls();
                CleanupExpiredCalls();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    RpcClient::~RpcClient()
    {
        Disconnect();
        
        ImplPtr->ShouldStop = true;
        if (ImplPtr->WorkerThread.joinable())
        {
            ImplPtr->WorkerThread.join();
        }
    }

    RpcResult RpcClient::Connect(const Network::NetworkAddress& ServerAddress)
    {
        if (ImplPtr->IsConnected)
        {
            return RpcResult::SUCCESS;
        }
        
        ImplPtr->ServerAddress = ServerAddress;
        
        // Create connection
        ImplPtr->ConnectionId = ImplPtr->NetworkManager->ConnectToServer(ServerAddress, Network::ProtocolType::TCP);
        if (ImplPtr->ConnectionId == Network::InvalidConnectionId)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Set up message handler
        ImplPtr->NetworkManager->SetOnDataReceivedCallback([this](Network::ConnectionId ConnId, const uint8_t* Data, size_t Size) {
            if (ConnId == ImplPtr->ConnectionId)
            {
                // Parse received data as RPC message
                std::string DataStr(reinterpret_cast<const char*>(Data), Size);
                auto Message = Message::Message::Create(Message::MessageType::CUSTOM_MESSAGE_START);
                Message->SetPayload(DataStr);
                
                RpcMessage RpcMsg(Message);
                HandleResponse(RpcMsg);
            }
        });
        
        ImplPtr->IsConnected = true;
        
        if (ImplPtr->ConnectionStateHandler)
        {
            ImplPtr->ConnectionStateHandler(true);
        }
        
        return RpcResult::SUCCESS;
    }

    void RpcClient::Disconnect()
    {
        if (!ImplPtr->IsConnected)
        {
            return;
        }
        
        if (ImplPtr->ConnectionId != Network::InvalidConnectionId)
        {
            ImplPtr->NetworkManager->DisconnectClient(ImplPtr->ConnectionId);
            ImplPtr->ConnectionId = Network::InvalidConnectionId;
        }
        
        ImplPtr->IsConnected = false;
        
        // Cancel all pending calls
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            for (auto& [CallId, Promise] : ImplPtr->PendingCalls)
            {
                Promise.set_value({RpcResult::NETWORK_ERROR, ""});
            }
            ImplPtr->PendingCalls.clear();
            
            for (auto& [CallId, Callback] : ImplPtr->AsyncCallbacks)
            {
                Callback(RpcResult::NETWORK_ERROR, "");
            }
            ImplPtr->AsyncCallbacks.clear();
            ImplPtr->CallTimestamps.clear();
        }
        
        if (ImplPtr->ConnectionStateHandler)
        {
            ImplPtr->ConnectionStateHandler(false);
        }
    }

    bool RpcClient::IsConnected() const
    {
        return ImplPtr->IsConnected;
    }

    void RpcClient::SetConfig(const RpcConfig& Config)
    {
        ImplPtr->Config = Config;
    }

    RpcConfig RpcClient::GetConfig() const
    {
        return ImplPtr->Config;
    }

    RpcResult RpcClient::Call(const std::string& ServiceName,
                             const std::string& MethodName,
                             const std::string& Parameters,
                             std::string& Result,
                             uint32_t TimeoutMs)
    {
        if (!ImplPtr->IsConnected)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Create RPC context
        RpcContext Context;
        Context.CallId = GenerateCallId();
        Context.ServiceName = ServiceName;
        Context.MethodName = MethodName;
        Context.CallType = RpcCallType::REQUEST;
        Context.TimeoutMs = (TimeoutMs > 0) ? TimeoutMs : ImplPtr->Config.DefaultTimeoutMs;
        Context.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create RPC message
        RpcMessage Request(Context);
        Request.SetParameters(Parameters);
        
        // Set up promise for synchronous waiting
        RpcPromise<std::pair<RpcResult, std::string>> Promise;
        auto Future = Promise.get_future();
        
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            ImplPtr->PendingCalls[Context.CallId] = std::move(Promise);
            ImplPtr->CallTimestamps[Context.CallId] = std::chrono::steady_clock::now();
        }
        
        // Send request
        RpcResult SendResult = SendRequest(Request, Context.TimeoutMs);
        if (SendResult != RpcResult::SUCCESS)
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            ImplPtr->PendingCalls.erase(Context.CallId);
            ImplPtr->CallTimestamps.erase(Context.CallId);
            return SendResult;
        }
        
        // Wait for response
        auto Status = Future.wait_for(std::chrono::milliseconds(Context.TimeoutMs));
        if (Status == std::future_status::timeout)
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            ImplPtr->PendingCalls.erase(Context.CallId);
            ImplPtr->CallTimestamps.erase(Context.CallId);
            
            std::lock_guard<std::mutex> StatsLock(ImplPtr->StatsMutex);
            ImplPtr->Stats.TimeoutCalls++;
            ImplPtr->Stats.FailedCalls++;
            
            return RpcResult::TIMEOUT;
        }
        
        auto [ResultCode, ResultData] = Future.get();
        Result = ResultData;
        
        // Update statistics
        {
            std::lock_guard<std::mutex> StatsLock(ImplPtr->StatsMutex);
            ImplPtr->Stats.TotalCalls++;
            if (ResultCode == RpcResult::SUCCESS)
            {
                ImplPtr->Stats.SuccessfulCalls++;
            }
            else
            {
                ImplPtr->Stats.FailedCalls++;
            }
        }
        
        return ResultCode;
    }

    RpcResult RpcClient::CallAsync(const std::string& ServiceName,
                                  const std::string& MethodName,
                                  const std::string& Parameters,
                                  RpcCallback Callback,
                                  uint32_t TimeoutMs)
    {
        if (!ImplPtr->IsConnected)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Create RPC context
        RpcContext Context;
        Context.CallId = GenerateCallId();
        Context.ServiceName = ServiceName;
        Context.MethodName = MethodName;
        Context.CallType = RpcCallType::REQUEST;
        Context.TimeoutMs = (TimeoutMs > 0) ? TimeoutMs : ImplPtr->Config.DefaultTimeoutMs;
        Context.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create RPC message
        RpcMessage Request(Context);
        Request.SetParameters(Parameters);
        
        // Store callback
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            ImplPtr->AsyncCallbacks[Context.CallId] = Callback;
            ImplPtr->CallTimestamps[Context.CallId] = std::chrono::steady_clock::now();
        }
        
        // Send request
        RpcResult SendResult = SendRequest(Request, Context.TimeoutMs);
        if (SendResult != RpcResult::SUCCESS)
        {
            std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
            ImplPtr->AsyncCallbacks.erase(Context.CallId);
            ImplPtr->CallTimestamps.erase(Context.CallId);
        }
        
        return SendResult;
    }

    RpcFuture<std::pair<RpcResult, std::string>> RpcClient::CallFuture(
        const std::string& ServiceName,
        const std::string& MethodName,
        const std::string& Parameters,
        uint32_t TimeoutMs)
    {
        auto Promise = std::make_shared<RpcPromise<std::pair<RpcResult, std::string>>>();
        auto Future = Promise->get_future();
        
        auto Callback = [Promise](RpcResult Result, const std::string& Response) {
            Promise->set_value({Result, Response});
        };
        
        RpcResult CallResult = CallAsync(ServiceName, MethodName, Parameters, Callback, TimeoutMs);
        if (CallResult != RpcResult::SUCCESS)
        {
            Promise->set_value({CallResult, ""});
        }
        
        return Future;
    }

    RpcResult RpcClient::Notify(const std::string& ServiceName,
                               const std::string& MethodName,
                               const std::string& Parameters)
    {
        if (!ImplPtr->IsConnected)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        // Create RPC context for notification
        RpcContext Context;
        Context.CallId = GenerateCallId();
        Context.ServiceName = ServiceName;
        Context.MethodName = MethodName;
        Context.CallType = RpcCallType::NOTIFICATION;
        Context.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Create RPC message
        RpcMessage Request(Context);
        Request.SetParameters(Parameters);
        
        // Send notification (no response expected)
        return SendRequest(Request, 0);
    }

    RpcResult RpcClient::BatchCall(const std::vector<RpcContext>& Calls,
                                  std::vector<std::pair<RpcResult, std::string>>& Results,
                                  uint32_t TimeoutMs)
    {
        // TODO: Implement batch call functionality
        // For now, make individual calls
        Results.clear();
        Results.reserve(Calls.size());
        
        for (const auto& Call : Calls)
        {
            std::string Result;
            RpcResult CallResult = this->Call(Call.ServiceName, Call.MethodName, "", Result, TimeoutMs);
            Results.emplace_back(CallResult, Result);
        }
        
        return RpcResult::SUCCESS;
    }

    RpcStats RpcClient::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        return ImplPtr->Stats;
    }

    void RpcClient::ResetStats()
    {
        std::lock_guard<std::mutex> Lock(ImplPtr->StatsMutex);
        ImplPtr->Stats = RpcStats{};
    }

    void RpcClient::SetConnectionStateHandler(std::function<void(bool)> Handler)
    {
        ImplPtr->ConnectionStateHandler = Handler;
    }

    void RpcClient::SetErrorHandler(RpcErrorHandler Handler)
    {
        ImplPtr->ErrorHandler = Handler;
    }

    void RpcClient::EnableServiceDiscovery(bool Enable)
    {
        // TODO: Implement service discovery integration
    }

    std::vector<std::string> RpcClient::GetAvailableServices()
    {
        // TODO: Implement service discovery integration
        return {};
    }

    // Private methods
    RpcId RpcClient::GenerateCallId()
    {
        return ImplPtr->NextCallId.fetch_add(1, std::memory_order_relaxed);
    }

    RpcResult RpcClient::SendRequest(const RpcMessage& Request, uint32_t TimeoutMs)
    {
        auto Message = Request.ToMessage();
        std::string Payload = Message->GetJsonPayload();
        
        Network::NetworkError SendResult = ImplPtr->NetworkManager->SendToClient(
            ImplPtr->ConnectionId,
            reinterpret_cast<const uint8_t*>(Payload.data()), 
            Payload.size()
        );
        
        if (SendResult != Network::NetworkError::SUCCESS)
        {
            return RpcResult::NETWORK_ERROR;
        }
        
        return RpcResult::SUCCESS;
    }

    void RpcClient::HandleResponse(const RpcMessage& Response)
    {
        RpcId CallId = Response.GetContext().CallId;
        
        std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
        
        // Handle synchronous calls
        auto PendingIt = ImplPtr->PendingCalls.find(CallId);
        if (PendingIt != ImplPtr->PendingCalls.end())
        {
            RpcResult Result = Response.IsError() ? Response.GetErrorCode() : RpcResult::SUCCESS;
            std::string ResultData = Response.IsError() ? "" : Response.GetResult();
            
            PendingIt->second.set_value({Result, ResultData});
            ImplPtr->PendingCalls.erase(PendingIt);
            ImplPtr->CallTimestamps.erase(CallId);
            return;
        }
        
        // Handle asynchronous calls
        auto AsyncIt = ImplPtr->AsyncCallbacks.find(CallId);
        if (AsyncIt != ImplPtr->AsyncCallbacks.end())
        {
            RpcResult Result = Response.IsError() ? Response.GetErrorCode() : RpcResult::SUCCESS;
            std::string ResultData = Response.IsError() ? "" : Response.GetResult();
            
            AsyncIt->second(Result, ResultData);
            ImplPtr->AsyncCallbacks.erase(AsyncIt);
            ImplPtr->CallTimestamps.erase(CallId);
        }
    }

    void RpcClient::ProcessPendingCalls()
    {
        // This method is called by the worker thread to handle incoming responses
        // In a real implementation, this would process network messages
    }

    void RpcClient::CleanupExpiredCalls()
    {
        auto Now = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> Lock(ImplPtr->CallsMutex);
        
        std::vector<RpcId> ExpiredCalls;
        
        for (const auto& [CallId, Timestamp] : ImplPtr->CallTimestamps)
        {
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Now - Timestamp);
            if (Elapsed.count() > ImplPtr->Config.DefaultTimeoutMs)
            {
                ExpiredCalls.push_back(CallId);
            }
        }
        
        for (RpcId CallId : ExpiredCalls)
        {
            // Handle expired synchronous calls
            auto PendingIt = ImplPtr->PendingCalls.find(CallId);
            if (PendingIt != ImplPtr->PendingCalls.end())
            {
                PendingIt->second.set_value({RpcResult::TIMEOUT, ""});
                ImplPtr->PendingCalls.erase(PendingIt);
            }
            
            // Handle expired asynchronous calls
            auto AsyncIt = ImplPtr->AsyncCallbacks.find(CallId);
            if (AsyncIt != ImplPtr->AsyncCallbacks.end())
            {
                AsyncIt->second(RpcResult::TIMEOUT, "");
                ImplPtr->AsyncCallbacks.erase(AsyncIt);
            }
            
            ImplPtr->CallTimestamps.erase(CallId);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> StatsLock(ImplPtr->StatsMutex);
                ImplPtr->Stats.TimeoutCalls++;
                ImplPtr->Stats.FailedCalls++;
            }
        }
    }

} // namespace Helianthus::RPC