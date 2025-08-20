#include "NetworkAdapterFixed.h"

#include <algorithm>
#include <iostream>
#include <random>

namespace Helianthus::RPC
{
NetworkAdapterFixed::NetworkAdapterFixed()
{
    // Initialize with default config
    Config.MaxConnections = 100;
    Config.ConnectionTimeoutMs = 5000;
    Config.BufferSizeBytes = 64 * 1024;  // 64KB
    Config.ThreadPoolSize = 2;
}

NetworkAdapterFixed::~NetworkAdapterFixed()
{
    Shutdown();
}

Network::NetworkError NetworkAdapterFixed::Initialize(const Network::NetworkConfig& InputConfig)
{
    if (IsInitialized)
    {
        return Network::NetworkError::ALREADY_INITIALIZED;
    }

    Config = InputConfig;
    IsInitialized = true;

    StartMessageProcessingThread();

    std::cout << "[NetworkAdapter] Initialized successfully" << std::endl;
    return Network::NetworkError::SUCCESS;
}

void NetworkAdapterFixed::Shutdown()
{
    if (!IsInitialized)
    {
        return;
    }

    StopServer();
    StopMessageProcessingThread();

    // Clear all connections
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        Connections.clear();
    }

    // Clear message queues
    {
        std::lock_guard<std::mutex> Lock(IncomingMutex);
        std::queue<InternalMessage> Empty;
        IncomingMessages.swap(Empty);
    }

    {
        std::lock_guard<std::mutex> Lock(OutgoingMutex);
        std::queue<InternalMessage> Empty;
        OutgoingMessages.swap(Empty);
    }

    IsInitialized = false;
    std::cout << "[NetworkAdapter] Shutdown completed" << std::endl;
}

Network::NetworkError NetworkAdapterFixed::StartServer(const Network::NetworkAddress& Address,
                                                       Network::ProtocolType Protocol)
{
    if (!IsInitialized)
    {
        return Network::NetworkError::NOT_INITIALIZED;
    }

    if (IsServerRunning)
    {
        return Network::NetworkError::SERVER_ALREADY_RUNNING;
    }

    ServerAddress = Address;
    IsServerRunning = true;

    std::cout << "[NetworkAdapter] 服务器启动在 " << Address.ToString() << std::endl;
    return Network::NetworkError::SUCCESS;
}

void NetworkAdapterFixed::StopServer()
{
    if (!IsServerRunning)
    {
        return;
    }

    IsServerRunning = false;

    // Disconnect all server connections
    std::vector<Network::ConnectionId> ServerConnections;
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        for (const auto& [ConnId, ConnInfo] : Connections)
        {
            if (ConnInfo.IsServerConnection)
            {
                ServerConnections.push_back(ConnId);
            }
        }
    }

    for (Network::ConnectionId ConnId : ServerConnections)
    {
        DisconnectClient(ConnId);
    }

    std::cout << "[NetworkAdapter] 服务器已停止" << std::endl;
}

Network::ConnectionId NetworkAdapterFixed::ConnectToServer(const Network::NetworkAddress& Address,
                                                           Network::ProtocolType Protocol)
{
    if (!IsInitialized)
    {
        return Network::InvalidConnectionId;
    }

    Network::ConnectionId ConnId = GenerateConnectionId();

    // Create connection info
    ConnectionInfo ConnInfo;
    ConnInfo.Address = Address;
    ConnInfo.IsServerConnection = false;  // Client connection
    ConnInfo.LastActivity = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        Connections[ConnId] = ConnInfo;
    }

    std::cout << "[NetworkAdapter] 客户端连接到 " << Address.ToString() << ", 连接ID: " << ConnId
              << std::endl;

    // Create a corresponding server-side connection to simulate bidirectional communication
    static std::unordered_map<std::string, std::weak_ptr<NetworkAdapterFixed>> ServerInstances;

    // Find server instance for this address (simplified simulation)
    std::string AddressKey = Address.ToString();

    // For demo: simulate server accepting connection
    std::thread(
        [this, ConnId, Address]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (ClientConnectedCallback)
            {
                ClientConnectedCallback(ConnId);
            }
        })
        .detach();

    return ConnId;
}

void NetworkAdapterFixed::DisconnectClient(Network::ConnectionId ClientId)
{
    if (!IsInitialized || ClientId == Network::InvalidConnectionId)
    {
        return;
    }

    bool ConnectionExisted = false;
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        auto It = Connections.find(ClientId);
        if (It != Connections.end())
        {
            ConnectionExisted = true;
            Connections.erase(It);
        }
    }

    if (ConnectionExisted)
    {
        std::cout << "[NetworkAdapter] 断开连接 ID: " << ClientId << std::endl;

        if (ClientDisconnectedCallback)
        {
            ClientDisconnectedCallback(ClientId, Network::NetworkError::SUCCESS);
        }
    }
}

Network::NetworkError
NetworkAdapterFixed::SendToClient(Network::ConnectionId ClientId, const char* Data, size_t Size)
{
    if (!IsInitialized || ClientId == Network::InvalidConnectionId || !Data || Size == 0)
    {
        return Network::NetworkError::INVALID_ADDRESS;
    }

    // Check if connection exists
    bool ConnectionExists = false;
    bool IsServerConnection = false;
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        auto It = Connections.find(ClientId);
        if (It != Connections.end())
        {
            ConnectionExists = true;
            IsServerConnection = It->second.IsServerConnection;
        }
    }

    if (!ConnectionExists)
    {
        return Network::NetworkError::CONNECTION_NOT_FOUND;
    }

    // Create message for processing
    InternalMessage Message;
    Message.ConnectionId = ClientId;
    Message.Data.assign(Data, Data + Size);
    Message.Timestamp = std::chrono::steady_clock::now();

    std::cout << "[NetworkAdapter] 发送消息到连接 " << ClientId << ", 大小: " << Size << " 字节"
              << std::endl;

    // For demo purposes: if this is a server sending to client, deliver immediately
    // If this is a client sending to server, it should be processed by the server first
    if (IsServerConnection)
    {
        // Server is sending response to client - deliver directly
        std::thread(
            [this, Message]()
            {
                SimulateNetworkDelay();
                if (DataReceivedCallback)
                {
                    std::cout << "[NetworkAdapter] 服务器响应传递到客户端，连接: "
                              << Message.ConnectionId << ", 大小: " << Message.Data.size()
                              << " 字节" << std::endl;
                    DataReceivedCallback(
                        Message.ConnectionId, Message.Data.data(), Message.Data.size());
                }
            })
            .detach();
    }
    else
    {
        // Client is sending to server - add to processing queue
        {
            std::lock_guard<std::mutex> Lock(IncomingMutex);
            IncomingMessages.push(Message);
        }
        IncomingCondition.notify_one();
    }

    return Network::NetworkError::SUCCESS;
}

void NetworkAdapterFixed::ProcessNetworkEvents()
{
    if (!IsInitialized)
    {
        return;
    }

    ProcessIncomingMessages();
    ProcessOutgoingMessages();
}

void NetworkAdapterFixed::SetOnClientConnectedCallback(
    std::function<void(Network::ConnectionId)> Callback)
{
    ClientConnectedCallback = Callback;
}

void NetworkAdapterFixed::SetOnClientDisconnectedCallback(
    std::function<void(Network::ConnectionId, Network::NetworkError)> Callback)
{
    ClientDisconnectedCallback = Callback;
}

void NetworkAdapterFixed::SetOnDataReceivedCallback(
    std::function<void(Network::ConnectionId, const char*, size_t)> Callback)
{
    DataReceivedCallback = Callback;
}

// Private methods

void NetworkAdapterFixed::StartMessageProcessingThread()
{
    ShouldStop = false;
    MessageProcessingThread = std::thread(&NetworkAdapterFixed::MessageProcessingLoop, this);
}

void NetworkAdapterFixed::StopMessageProcessingThread()
{
    ShouldStop = true;
    IncomingCondition.notify_all();

    if (MessageProcessingThread.joinable())
    {
        MessageProcessingThread.join();
    }
}

void NetworkAdapterFixed::MessageProcessingLoop()
{
    std::cout << "[NetworkAdapter] 消息处理线程启动" << std::endl;

    while (!ShouldStop)
    {
        ProcessIncomingMessages();
        ProcessOutgoingMessages();

        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[NetworkAdapter] 消息处理线程停止" << std::endl;
}

void NetworkAdapterFixed::ProcessIncomingMessages()
{
    std::unique_lock<std::mutex> Lock(IncomingMutex);

    while (!IncomingMessages.empty())
    {
        InternalMessage Message = IncomingMessages.front();
        IncomingMessages.pop();
        Lock.unlock();

        // Simulate network delay
        SimulateNetworkDelay();

        // Check if connection still exists
        bool ConnectionExists = false;
        {
            std::lock_guard<std::mutex> ConnLock(ConnectionMutex);
            auto It = Connections.find(Message.ConnectionId);
            if (It != Connections.end())
            {
                ConnectionExists = true;
                It->second.LastActivity = std::chrono::steady_clock::now();
            }
        }

        if (ConnectionExists && DataReceivedCallback)
        {
            std::cout << "[NetworkAdapter] 处理接收消息，连接: " << Message.ConnectionId
                      << ", 大小: " << Message.Data.size() << " 字节" << std::endl;

            DataReceivedCallback(Message.ConnectionId, Message.Data.data(), Message.Data.size());
        }

        Lock.lock();
    }
}

void NetworkAdapterFixed::ProcessOutgoingMessages()
{
    std::lock_guard<std::mutex> Lock(OutgoingMutex);

    while (!OutgoingMessages.empty())
    {
        InternalMessage Message = OutgoingMessages.front();
        OutgoingMessages.pop();

        // For demo purposes, outgoing messages are processed immediately
        std::cout << "[NetworkAdapter] 处理发送消息，连接: " << Message.ConnectionId
                  << ", 大小: " << Message.Data.size() << " 字节" << std::endl;
    }
}

Network::ConnectionId NetworkAdapterFixed::GenerateConnectionId()
{
    return NextConnectionId.fetch_add(1, std::memory_order_relaxed);
}

void NetworkAdapterFixed::SimulateNetworkDelay()
{
    // Simulate realistic network delay (0-5ms)
    static thread_local std::random_device RandomDevice;
    static thread_local std::mt19937 Generator(RandomDevice());
    static thread_local std::uniform_int_distribution<int> Distribution(0, 5);

    int DelayMs = Distribution(Generator);
    if (DelayMs > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(DelayMs));
    }
}

}  // namespace Helianthus::RPC