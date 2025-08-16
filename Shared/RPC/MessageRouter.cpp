#include "MessageRouter.h"
#include <iostream>
#include <chrono>

namespace Helianthus::RPC
{
    MessageRouter& MessageRouter::GetInstance()
    {
        static MessageRouter Instance;
        return Instance;
    }

    void MessageRouter::Initialize()
    {
        if (IsInitialized)
        {
            return;
        }

        ShouldStop = false;
        ProcessingThread = std::thread(&MessageRouter::ProcessMessages, this);
        IsInitialized = true;
        
        std::cout << "[MessageRouter] 全局消息路由器启动" << std::endl;
    }

    void MessageRouter::Shutdown()
    {
        if (!IsInitialized)
        {
            return;
        }

        ShouldStop = true;
        MessageCondition.notify_all();
        
        if (ProcessingThread.joinable())
        {
            ProcessingThread.join();
        }
        
        {
            std::lock_guard<std::mutex> ServerLock(ServerMutex);
            ServerCallbacks.clear();
        }
        
        {
            std::lock_guard<std::mutex> ClientLock(ClientMutex);
            ClientCallbacks.clear();
        }
        
        IsInitialized = false;
        std::cout << "[MessageRouter] 全局消息路由器关闭" << std::endl;
    }

    void MessageRouter::RegisterServer(const std::string& Address, MessageCallback Callback)
    {
        std::lock_guard<std::mutex> Lock(ServerMutex);
        ServerCallbacks[Address] = Callback;
        std::cout << "[MessageRouter] 注册服务器: " << Address << std::endl;
    }

    void MessageRouter::UnregisterServer(const std::string& Address)
    {
        std::lock_guard<std::mutex> Lock(ServerMutex);
        ServerCallbacks.erase(Address);
        std::cout << "[MessageRouter] 注销服务器: " << Address << std::endl;
    }

    void MessageRouter::RegisterClient(Network::ConnectionId ClientId, MessageCallback Callback)
    {
        std::lock_guard<std::mutex> Lock(ClientMutex);
        ClientCallbacks[ClientId] = Callback;
        std::cout << "[MessageRouter] 注册客户端: " << ClientId << std::endl;
    }

    void MessageRouter::UnregisterClient(Network::ConnectionId ClientId)
    {
        std::lock_guard<std::mutex> Lock(ClientMutex);
        ClientCallbacks.erase(ClientId);
        std::cout << "[MessageRouter] 注销客户端: " << ClientId << std::endl;
    }

    void MessageRouter::SendToServer(const std::string& ServerAddress, Network::ConnectionId ClientId,
                                   const uint8_t* Data, size_t Size)
    {
        if (!IsInitialized || !Data || Size == 0)
        {
            return;
        }

        Message Msg;
        Msg.FromConnection = ClientId;
        Msg.ToConnection = Network::InvalidConnectionId; // Server doesn't have connection ID
        Msg.ServerAddress = ServerAddress;
        Msg.Data.assign(Data, Data + Size);
        Msg.Timestamp = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> Lock(MessageMutex);
            PendingMessages.push(Msg);
        }
        MessageCondition.notify_one();

        std::cout << "[MessageRouter] 客户端 " << ClientId << " 发送消息到服务器 " 
                  << ServerAddress << ", 大小: " << Size << " 字节" << std::endl;
    }

    void MessageRouter::SendToClient(Network::ConnectionId ClientId, const uint8_t* Data, size_t Size)
    {
        if (!IsInitialized || ClientId == Network::InvalidConnectionId || !Data || Size == 0)
        {
            return;
        }

        Message Msg;
        Msg.FromConnection = Network::InvalidConnectionId; // From server
        Msg.ToConnection = ClientId;
        Msg.ServerAddress = "";
        Msg.Data.assign(Data, Data + Size);
        Msg.Timestamp = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> Lock(MessageMutex);
            PendingMessages.push(Msg);
        }
        MessageCondition.notify_one();

        std::cout << "[MessageRouter] 服务器发送响应到客户端 " << ClientId 
                  << ", 大小: " << Size << " 字节" << std::endl;
    }

    Network::ConnectionId MessageRouter::CreateServerConnection(const std::string& ServerAddress)
    {
        // Generate a unique connection ID for this server connection
        static std::atomic<Network::ConnectionId> NextConnId{1000};
        Network::ConnectionId ConnId = NextConnId.fetch_add(1);
        
        std::cout << "[MessageRouter] 为服务器 " << ServerAddress 
                  << " 创建连接 ID: " << ConnId << std::endl;
        
        return ConnId;
    }

    void MessageRouter::ProcessMessages()
    {
        std::cout << "[MessageRouter] 消息处理线程启动" << std::endl;
        
        while (!ShouldStop)
        {
            std::unique_lock<std::mutex> Lock(MessageMutex);
            MessageCondition.wait(Lock, [this] { return !PendingMessages.empty() || ShouldStop; });
            
            if (ShouldStop)
            {
                break;
            }
            
            while (!PendingMessages.empty())
            {
                Message Msg = PendingMessages.front();
                PendingMessages.pop();
                Lock.unlock();
                
                // Simulate network delay
                auto Now = std::chrono::steady_clock::now();
                auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Now - Msg.Timestamp);
                if (Elapsed.count() < 5) // Minimum 5ms delay
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5 - Elapsed.count()));
                }
                
                // Route message
                if (!Msg.ServerAddress.empty())
                {
                    // Message to server
                    std::lock_guard<std::mutex> ServerLock(ServerMutex);
                    auto It = ServerCallbacks.find(Msg.ServerAddress);
                    if (It != ServerCallbacks.end())
                    {
                        std::cout << "[MessageRouter] 路由消息到服务器 " << Msg.ServerAddress << std::endl;
                        It->second(Msg.FromConnection, Msg.Data.data(), Msg.Data.size());
                    }
                    else
                    {
                        std::cout << "[MessageRouter] 服务器 " << Msg.ServerAddress << " 未找到" << std::endl;
                    }
                }
                else if (Msg.ToConnection != Network::InvalidConnectionId)
                {
                    // Message to client
                    std::lock_guard<std::mutex> ClientLock(ClientMutex);
                    auto It = ClientCallbacks.find(Msg.ToConnection);
                    if (It != ClientCallbacks.end())
                    {
                        std::cout << "[MessageRouter] 路由消息到客户端 " << Msg.ToConnection << std::endl;
                        It->second(Msg.ToConnection, Msg.Data.data(), Msg.Data.size());
                    }
                    else
                    {
                        std::cout << "[MessageRouter] 客户端 " << Msg.ToConnection << " 未找到" << std::endl;
                    }
                }
                
                Lock.lock();
            }
        }
        
        std::cout << "[MessageRouter] 消息处理线程停止" << std::endl;
    }

} // namespace Helianthus::RPC