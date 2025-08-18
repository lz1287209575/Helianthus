#include "NetworkAdapterSimple.h"
#include <iostream>

namespace Helianthus::RPC
{
    NetworkAdapterSimple::NetworkAdapterSimple()
    {
        // Initialize global message router
        MessageRouter::GetInstance().Initialize();
    }

    NetworkAdapterSimple::~NetworkAdapterSimple()
    {
        Shutdown();
    }

    Network::NetworkError NetworkAdapterSimple::Initialize(const Network::NetworkConfig& Config)
    {
        if (IsInitialized)
        {
            return Network::NetworkError::ALREADY_INITIALIZED;
        }

        IsInitialized = true;
        std::cout << "[NetworkAdapterSimple] Initialized successfully" << std::endl;
        return Network::NetworkError::SUCCESS;
    }

    void NetworkAdapterSimple::Shutdown()
    {
        if (!IsInitialized)
        {
            return;
        }

        StopServer();
        
        if (ClientConnectionId != Network::InvalidConnectionId)
        {
            MessageRouter::GetInstance().UnregisterClient(ClientConnectionId);
            ClientConnectionId = Network::InvalidConnectionId;
        }

        IsInitialized = false;
        std::cout << "[NetworkAdapterSimple] Shutdown completed" << std::endl;
    }

    Network::NetworkError NetworkAdapterSimple::StartServer(const Network::NetworkAddress& Address, Network::ProtocolType Protocol)
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
        
        // Register with message router
        auto Callback = [this](Network::ConnectionId ConnId, const char* Data, size_t Size) {
            HandleIncomingMessage(ConnId, Data, Size);
        };
        
        MessageRouter::GetInstance().RegisterServer(Address.ToString(), Callback);
        
        std::cout << "[NetworkAdapterSimple] 服务器启动在 " << Address.ToString() << std::endl;
        return Network::NetworkError::SUCCESS;
    }

    void NetworkAdapterSimple::StopServer()
    {
        if (!IsServerRunning)
        {
            return;
        }

        MessageRouter::GetInstance().UnregisterServer(ServerAddress.ToString());
        IsServerRunning = false;
        
        std::cout << "[NetworkAdapterSimple] 服务器已停止" << std::endl;
    }

    Network::ConnectionId NetworkAdapterSimple::ConnectToServer(const Network::NetworkAddress& Address, Network::ProtocolType Protocol)
    {
        if (!IsInitialized)
        {
            return Network::InvalidConnectionId;
        }

        // Store server address for client
        ServerAddress = Address;
        ClientConnectionId = MessageRouter::GetInstance().CreateServerConnection(Address.ToString());
        
        // Register client callback
        auto Callback = [this](Network::ConnectionId ConnId, const char* Data, size_t Size) {
            HandleIncomingMessage(ConnId, Data, Size);
        };
        
        MessageRouter::GetInstance().RegisterClient(ClientConnectionId, Callback);
        
        std::cout << "[NetworkAdapterSimple] 客户端连接到 " << Address.ToString() 
                  << ", 连接ID: " << ClientConnectionId << std::endl;

        // Notify connection established
        if (ClientConnectedCallback)
        {
            ClientConnectedCallback(ClientConnectionId);
        }

        return ClientConnectionId;
    }

    void NetworkAdapterSimple::DisconnectClient(Network::ConnectionId ClientId)
    {
        if (!IsInitialized || ClientId == Network::InvalidConnectionId)
        {
            return;
        }

        if (ClientId == ClientConnectionId)
        {
            MessageRouter::GetInstance().UnregisterClient(ClientConnectionId);
            ClientConnectionId = Network::InvalidConnectionId;
        }

        std::cout << "[NetworkAdapterSimple] 断开连接 ID: " << ClientId << std::endl;
        
        if (ClientDisconnectedCallback)
        {
            ClientDisconnectedCallback(ClientId, Network::NetworkError::SUCCESS);
        }
    }

    Network::NetworkError NetworkAdapterSimple::SendToClient(Network::ConnectionId ClientId, const char* Data, size_t Size)
    {
        if (!IsInitialized || ClientId == Network::InvalidConnectionId || !Data || Size == 0)
        {
            return Network::NetworkError::INVALID_ADDRESS;
        }

        if (IsServerRunning)
        {
            // Server sending to client
            MessageRouter::GetInstance().SendToClient(ClientId, Data, Size);
        }
        else
        {
            // Client sending to server - need to determine server address from connection
            // For now, we'll store the server address when client connects
            if (!ServerAddress.IsValid())
            {
                std::cout << "[NetworkAdapterSimple] 错误: 客户端没有有效的服务器地址" << std::endl;
                return Network::NetworkError::INVALID_ADDRESS;
            }
            MessageRouter::GetInstance().SendToServer(ServerAddress.ToString(), ClientId, Data, Size);
        }

        return Network::NetworkError::SUCCESS;
    }

    void NetworkAdapterSimple::ProcessNetworkEvents()
    {
        // Events are processed by MessageRouter automatically
    }

    void NetworkAdapterSimple::SetOnClientConnectedCallback(std::function<void(Network::ConnectionId)> Callback)
    {
        ClientConnectedCallback = Callback;
    }

    void NetworkAdapterSimple::SetOnClientDisconnectedCallback(std::function<void(Network::ConnectionId, Network::NetworkError)> Callback)
    {
        ClientDisconnectedCallback = Callback;
    }

    void NetworkAdapterSimple::SetOnDataReceivedCallback(std::function<void(Network::ConnectionId, const char*, size_t)> Callback)
    {
        DataReceivedCallback = Callback;
    }

    void NetworkAdapterSimple::HandleIncomingMessage(Network::ConnectionId ConnId, const char* Data, size_t Size)
    {
        if (DataReceivedCallback)
        {
            std::cout << "[NetworkAdapterSimple] 接收消息，连接: " << ConnId 
                      << ", 大小: " << Size << " 字节" << std::endl;
            DataReceivedCallback(ConnId, Data, Size);
        }
    }

} // namespace Helianthus::RPC