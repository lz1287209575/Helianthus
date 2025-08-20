#include "NetworkAdapter.h"

#include <iostream>

#include "../Network/NetworkTypes.h"

namespace Helianthus::RPC
{
NetworkAdapter::NetworkAdapter()
{
    NetworkManager = std::make_shared<Network::NetworkManager>();
}

NetworkAdapter::~NetworkAdapter()
{
    Shutdown();
}

Network::NetworkError NetworkAdapter::Initialize(const Network::NetworkConfig& Config)
{
    if (IsInitialized)
    {
        return Network::NetworkError::ALREADY_INITIALIZED;
    }

    Network::NetworkError Result = NetworkManager->Initialize(Config);
    if (Result == Network::NetworkError::SUCCESS)
    {
        IsInitialized = true;
    }

    return Result;
}

void NetworkAdapter::Shutdown()
{
    if (!IsInitialized)
    {
        return;
    }

    StopServer();
    NetworkManager->Shutdown();
    IsInitialized = false;
}

Network::NetworkError NetworkAdapter::StartServer(const Network::NetworkAddress& Address,
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

    // Use the existing NetworkManager API
    Network::NetworkError Result = NetworkManager->StartServer(Address);
    if (Result == Network::NetworkError::SUCCESS)
    {
        IsServerRunning = true;
        ServerAddress = Address;
    }

    return Result;
}

void NetworkAdapter::StopServer()
{
    if (IsServerRunning)
    {
        NetworkManager->StopServer();
        IsServerRunning = false;
    }
}

Network::ConnectionId NetworkAdapter::ConnectToServer(const Network::NetworkAddress& Address,
                                                      Network::ProtocolType Protocol)
{
    if (!IsInitialized)
    {
        return Network::InvalidConnectionId;
    }

    // Use the existing NetworkManager API - this is simplified for now
    Network::ConnectionId ConnectionId = Network::InvalidConnectionId;
    Network::NetworkError Result = NetworkManager->CreateConnection(Address, ConnectionId);

    if (Result == Network::NetworkError::SUCCESS && ConnectionId != Network::InvalidConnectionId)
    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        ConnectionAddresses[ConnectionId] = Address;

        // Simulate successful connection
        if (ClientConnectedCallback)
        {
            ClientConnectedCallback(ConnectionId);
        }
    }

    return ConnectionId;
}

void NetworkAdapter::DisconnectClient(Network::ConnectionId ClientId)
{
    if (!IsInitialized)
    {
        return;
    }

    Network::NetworkError Result = NetworkManager->CloseConnection(ClientId);

    {
        std::lock_guard<std::mutex> Lock(ConnectionMutex);
        ConnectionAddresses.erase(ClientId);
    }

    // Notify disconnection
    if (ClientDisconnectedCallback)
    {
        ClientDisconnectedCallback(ClientId, Result);
    }
}

Network::NetworkError
NetworkAdapter::SendToClient(Network::ConnectionId ClientId, const char* Data, size_t Size)
{
    if (!IsInitialized)
    {
        return Network::NetworkError::NOT_INITIALIZED;
    }

    // Create a simple message wrapper for the existing API
    auto Message = Message::Message::Create(Message::MessageType::CUSTOM_MESSAGE_START);
    std::string DataStr(reinterpret_cast<const char*>(Data), Size);
    Message->SetPayload(DataStr);

    return NetworkManager->SendMessage(ClientId, *Message);
}

void NetworkAdapter::ProcessNetworkEvents()
{
    if (!IsInitialized)
    {
        return;
    }

    // Process incoming messages
    if (NetworkManager->HasIncomingMessages())
    {
        auto Messages = NetworkManager->GetAllMessages();
        for (const auto& Message : Messages)
        {
            if (Message && DataReceivedCallback)
            {
                std::string Payload = Message->GetJsonPayload();
                // Note: This is a simplified approach - in a real implementation,
                // we would need to properly track which connection sent which message
                Network::ConnectionId SenderId = 1;  // Placeholder
                DataReceivedCallback(
                    SenderId, reinterpret_cast<const char*>(Payload.data()), Payload.size());
            }
        }
    }
}

void NetworkAdapter::SetOnClientConnectedCallback(
    std::function<void(Network::ConnectionId)> Callback)
{
    ClientConnectedCallback = Callback;
}

void NetworkAdapter::SetOnClientDisconnectedCallback(
    std::function<void(Network::ConnectionId, Network::NetworkError)> Callback)
{
    ClientDisconnectedCallback = Callback;
}

void NetworkAdapter::SetOnDataReceivedCallback(
    std::function<void(Network::ConnectionId, const char*, size_t)> Callback)
{
    DataReceivedCallback = Callback;
}

}  // namespace Helianthus::RPC