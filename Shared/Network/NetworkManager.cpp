#include "Network/NetworkManager.h"

#include <algorithm>
#include <sstream>

namespace Helianthus::Network
{
NetworkManager::NetworkManager()
{
    Config = NetworkConfig{};
    Stats = NetworkStats{};

    // Initialize message queues
    // IncomingMessages and OutgoingMessages are now simple queues that don't need initialization
}

NetworkManager::~NetworkManager()
{
    Shutdown();
}

NetworkManager::NetworkManager(NetworkManager&& Other) noexcept
    : Config(std::move(Other.Config)),
      InitializedFlag(Other.InitializedFlag.load()),
      ShuttingDownFlag(Other.ShuttingDownFlag.load()),
      Connections(std::move(Other.Connections)),
      NextConnectionId(Other.NextConnectionId.load()),
      ConnectionGroups(std::move(Other.ConnectionGroups)),
      ServerRunningFlag(Other.ServerRunningFlag.load()),
      ServerSocket(std::move(Other.ServerSocket)),
      IncomingMessages(std::move(Other.IncomingMessages)),
      OutgoingMessages(std::move(Other.OutgoingMessages)),
      StopMessageProcessing(Other.StopMessageProcessing.load()),
      MessageHandlerFunc(std::move(Other.MessageHandlerFunc)),
      ConnectionHandlerFunc(std::move(Other.ConnectionHandlerFunc)),
      Stats(std::move(Other.Stats)),
      StopServerAccept(Other.StopServerAccept.load())
{
    Other.InitializedFlag = false;
    Other.ShuttingDownFlag = false;
}

NetworkManager& NetworkManager::operator=(NetworkManager&& Other) noexcept
{
    if (this != &Other)
    {
        Shutdown();

        Config = std::move(Other.Config);
        InitializedFlag = Other.InitializedFlag.load();
        ShuttingDownFlag = Other.ShuttingDownFlag.load();
        Connections = std::move(Other.Connections);
        NextConnectionId = Other.NextConnectionId.load();
        ConnectionGroups = std::move(Other.ConnectionGroups);
        ServerRunningFlag = Other.ServerRunningFlag.load();
        ServerSocket = std::move(Other.ServerSocket);
        IncomingMessages = std::move(Other.IncomingMessages);
        OutgoingMessages = std::move(Other.OutgoingMessages);
        StopMessageProcessing = Other.StopMessageProcessing.load();
        MessageHandlerFunc = std::move(Other.MessageHandlerFunc);
        ConnectionHandlerFunc = std::move(Other.ConnectionHandlerFunc);
        Stats = std::move(Other.Stats);
        StopServerAccept = Other.StopServerAccept.load();

        Other.InitializedFlag = false;
        Other.ShuttingDownFlag = false;
    }
    return *this;
}

NetworkError NetworkManager::Initialize(const NetworkConfig& Config)
{
    if (InitializedFlag)
    {
        return NetworkError::ALREADY_INITIALIZED;
    }

    this->Config = Config;
    InitializedFlag = true;
    ShuttingDownFlag = false;

    // Start message processing thread
    StartMessageProcessingThread();

    return NetworkError::SUCCESS;
}

void NetworkManager::Shutdown()
{
    if (!InitializedFlag)
    {
        return;
    }

    ShuttingDownFlag = true;

    // Stop server if running
    StopServer();

    // Stop message processing
    StopMessageProcessingThread();

    // Close all connections
    CloseAllConnections();

    // Clear handlers
    RemoveAllHandlers();

    InitializedFlag = false;
}

bool NetworkManager::IsInitialized() const
{
    return InitializedFlag;
}

NetworkError NetworkManager::CreateConnection(const NetworkAddress& Address,
                                              ConnectionId& OutConnectionId)
{
    if (!InitializedFlag || ShuttingDownFlag)
    {
        return NetworkError::NOT_INITIALIZED;
    }

    // Create new TCP socket
    auto Socket = std::make_unique<Sockets::TcpSocket>();
    auto Result = Socket->Connect(Address);

    if (Result != NetworkError::SUCCESS)
    {
        return Result;
    }

    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    // Generate connection ID
    OutConnectionId = GenerateConnectionId();

    // Create connection entry
    ConnectionEntry Entry;
    Entry.Socket = std::move(Socket);
    Entry.RemoteAddress = Address;
    Entry.State = ConnectionState::CONNECTED;
    Entry.CreationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    Entry.LastActivity = Entry.CreationTime;
    Entry.Stats = ConnectionStats{};

    Connections[OutConnectionId] = std::move(Entry);

    // Update statistics
    {
        std::lock_guard<std::mutex> StatsLock(StatsMutex);
        Stats.TotalConnectionsCreated++;
        Stats.ActiveConnections++;
    }

    // Notify connection established
    NotifyConnectionStateChange(OutConnectionId, NetworkError::SUCCESS);

    return NetworkError::SUCCESS;
}

NetworkError NetworkManager::CloseConnection(ConnectionId Id)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return NetworkError::CONNECTION_NOT_FOUND;
    }

    // Close the socket
    if (It->second.Socket)
    {
        It->second.Socket->Disconnect();
    }

    // Remove from all groups
    {
        std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
        for (auto& GroupPair : ConnectionGroups)
        {
            GroupPair.second.erase(Id);
        }
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> StatsLock(StatsMutex);
        Stats.ActiveConnections--;
        Stats.TotalConnectionsClosed++;
    }

    // Remove connection
    Connections.erase(It);

    // Notify connection closed
    NotifyConnectionStateChange(Id, NetworkError::CONNECTION_CLOSED);

    return NetworkError::SUCCESS;
}

NetworkError NetworkManager::CloseAllConnections()
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    for (auto& Pair : Connections)
    {
        if (Pair.second.Socket)
        {
            Pair.second.Socket->Disconnect();
        }
    }

    // Clear all connection groups
    {
        std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
        ConnectionGroups.clear();
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> StatsLock(StatsMutex);
        Stats.TotalConnectionsClosed += Connections.size();
        Stats.ActiveConnections = 0;
    }

    Connections.clear();
    return NetworkError::SUCCESS;
}

bool NetworkManager::IsConnectionActive(ConnectionId Id) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return false;
    }

    return It->second.IsActive();
}

ConnectionState NetworkManager::GetConnectionState(ConnectionId Id) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return ConnectionState::DISCONNECTED;
    }

    return It->second.State;
}

std::vector<ConnectionId> NetworkManager::GetActiveConnections() const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    std::vector<ConnectionId> ActiveIds;
    for (const auto& Pair : Connections)
    {
        if (Pair.second.IsActive())
        {
            ActiveIds.push_back(Pair.first);
        }
    }

    return ActiveIds;
}

NetworkError NetworkManager::StartServer(const NetworkAddress& BindAddress)
{
    if (ServerRunningFlag)
    {
        return NetworkError::SERVER_ALREADY_RUNNING;
    }

    ServerSocket = std::make_unique<Sockets::TcpSocket>();
    auto Result = ServerSocket->Bind(BindAddress);
    if (Result == NetworkError::SUCCESS)
    {
        Result = ServerSocket->Listen(128);
    }

    if (Result != NetworkError::SUCCESS)
    {
        ServerSocket.reset();
        return Result;
    }

    ServerRunningFlag = true;
    StartServerAcceptThread();

    return NetworkError::SUCCESS;
}

NetworkError NetworkManager::StopServer()
{
    if (!ServerRunningFlag)
    {
        return NetworkError::SUCCESS;
    }

    StopServerAcceptThread();

    if (ServerSocket)
    {
        ServerSocket->Disconnect();
        ServerSocket.reset();
    }

    ServerRunningFlag = false;
    return NetworkError::SUCCESS;
}

bool NetworkManager::IsServerRunning() const
{
    return ServerRunningFlag;
}

NetworkError NetworkManager::SendMessage(ConnectionId Id, const Message::Message& Msg)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    auto It = Connections.find(Id);
    if (It == Connections.end())
    {
        return NetworkError::CONNECTION_NOT_FOUND;
    }

    if (!It->second.IsActive())
    {
        return NetworkError::CONNECTION_CLOSED;
    }

    // Serialize message
    auto SerializedData = Msg.Serialize();
    if (SerializedData.empty())
    {
        return NetworkError::SERIALIZATION_FAILED;
    }

    // Send data
    size_t BytesSent = 0;
    auto Result = It->second.Socket->Send(SerializedData.data(), SerializedData.size(), BytesSent);
    if (Result == NetworkError::SUCCESS)
    {
        UpdateConnectionActivity(Id);

        // Update statistics
        std::lock_guard<std::mutex> StatsLock(StatsMutex);
        Stats.TotalMessagesSent++;
        Stats.TotalBytesSent += SerializedData.size();
    }

    return Result;
}

NetworkError NetworkManager::SendMessageReliable(ConnectionId Id, const Message::Message& Msg)
{
    // For now, same as regular send - could add retry logic later
    return SendMessage(Id, Msg);
}

NetworkError NetworkManager::BroadcastMessage(const Message::Message& Msg)
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);

    NetworkError LastError = NetworkError::SUCCESS;
    uint32_t SuccessCount = 0;

    for (const auto& Pair : Connections)
    {
        if (Pair.second.IsActive())
        {
            auto Result = SendMessage(Pair.first, Msg);
            if (Result == NetworkError::SUCCESS)
            {
                SuccessCount++;
            }
            else
            {
                LastError = Result;
            }
        }
    }

    return SuccessCount > 0 ? NetworkError::SUCCESS : LastError;
}

NetworkError NetworkManager::BroadcastMessageToGroup(const std::string& GroupName,
                                                     const Message::Message& Msg)
{
    std::lock_guard<std::mutex> GroupsLock(GroupsMutex);

    auto It = ConnectionGroups.find(GroupName);
    if (It == ConnectionGroups.end())
    {
        return NetworkError::GROUP_NOT_FOUND;
    }

    NetworkError LastError = NetworkError::SUCCESS;
    uint32_t SuccessCount = 0;

    for (ConnectionId Id : It->second)
    {
        if (IsConnectionActive(Id))
        {
            auto Result = SendMessage(Id, Msg);
            if (Result == NetworkError::SUCCESS)
            {
                SuccessCount++;
            }
            else
            {
                LastError = Result;
            }
        }
    }

    return SuccessCount > 0 ? NetworkError::SUCCESS : LastError;
}

// Message queue operations
bool NetworkManager::HasIncomingMessages() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return !IncomingMessages.empty();
}

Message::MessagePtr NetworkManager::GetNextMessage()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    if (IncomingMessages.empty())
    {
        return nullptr;
    }

    auto Msg = IncomingMessages.front();
    IncomingMessages.pop();
    return Msg;
}

std::vector<Message::MessagePtr> NetworkManager::GetAllMessages()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    std::vector<Message::MessagePtr> Messages;

    while (!IncomingMessages.empty())
    {
        Messages.push_back(IncomingMessages.front());
        IncomingMessages.pop();
    }

    return Messages;
}

uint32_t NetworkManager::GetIncomingMessageCount() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return static_cast<uint32_t>(IncomingMessages.size());
}

// Helper methods implementation
ConnectionId NetworkManager::GenerateConnectionId()
{
    return NextConnectionId.fetch_add(1, std::memory_order_relaxed);
}

void NetworkManager::UpdateConnectionActivity(ConnectionId Id)
{
    auto It = Connections.find(Id);
    if (It != Connections.end())
    {
        It->second.LastActivity = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
    }
}

void NetworkManager::NotifyMessageReceived(const Message::Message& Msg)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    if (MessageHandlerFunc)
    {
        MessageHandlerFunc(Msg);
    }
}

void NetworkManager::NotifyConnectionStateChange(ConnectionId Id, NetworkError Error)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    if (ConnectionHandlerFunc)
    {
        ConnectionHandlerFunc(Id, Error);
    }
}

void NetworkManager::SetMessageHandler(MessageHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    MessageHandlerFunc = std::move(Handler);
}

void NetworkManager::SetConnectionHandler(ConnectionHandler Handler)
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    ConnectionHandlerFunc = std::move(Handler);
}

void NetworkManager::RemoveAllHandlers()
{
    std::lock_guard<std::mutex> Lock(HandlersMutex);
    MessageHandlerFunc = nullptr;
    ConnectionHandlerFunc = nullptr;
}

NetworkStats NetworkManager::GetNetworkStats() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return Stats;
}

void NetworkManager::StartMessageProcessingThread()
{
    StopMessageProcessing = false;
    MessageProcessingThread = std::thread(&NetworkManager::MessageProcessingLoop, this);
}

void NetworkManager::StopMessageProcessingThread()
{
    StopMessageProcessing = true;
    if (MessageProcessingThread.joinable())
    {
        MessageProcessingThread.join();
    }
}

void NetworkManager::MessageProcessingLoop()
{
    while (!StopMessageProcessing && !ShuttingDownFlag)
    {
        // Process incoming messages
        while (HasIncomingMessages())
        {
            auto Msg = GetNextMessage();
            if (Msg)
            {
                NotifyMessageReceived(*Msg);
            }
        }

        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkManager::StartServerAcceptThread()
{
    StopServerAccept = false;
    ServerAcceptThread = std::thread(&NetworkManager::ServerAcceptLoop, this);
}

void NetworkManager::StopServerAcceptThread()
{
    StopServerAccept = true;
    if (ServerAcceptThread.joinable())
    {
        ServerAcceptThread.join();
    }
}

void NetworkManager::ServerAcceptLoop()
{
    while (!StopServerAccept && ServerRunningFlag)
    {
        AcceptConnection();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

NetworkError NetworkManager::AcceptConnection()
{
    // Basic implementation - would need platform-specific accept logic
    return NetworkError::SUCCESS;
}

// Stub implementations for remaining methods
NetworkError NetworkManager::AddConnectionToGroup(ConnectionId Id, const std::string& GroupName)
{
    std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
    ConnectionGroups[GroupName].insert(Id);
    return NetworkError::SUCCESS;
}

NetworkError NetworkManager::RemoveConnectionFromGroup(ConnectionId Id,
                                                       const std::string& GroupName)
{
    std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
    auto It = ConnectionGroups.find(GroupName);
    if (It != ConnectionGroups.end())
    {
        It->second.erase(Id);
        if (It->second.empty())
        {
            ConnectionGroups.erase(It);
        }
    }
    return NetworkError::SUCCESS;
}

std::vector<ConnectionId> NetworkManager::GetConnectionsInGroup(const std::string& GroupName) const
{
    std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
    auto It = ConnectionGroups.find(GroupName);
    if (It != ConnectionGroups.end())
    {
        return std::vector<ConnectionId>(It->second.begin(), It->second.end());
    }
    return {};
}

void NetworkManager::ClearGroup(const std::string& GroupName)
{
    std::lock_guard<std::mutex> GroupsLock(GroupsMutex);
    ConnectionGroups.erase(GroupName);
}

ConnectionStats NetworkManager::GetConnectionStats(ConnectionId Id) const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    auto It = Connections.find(Id);
    return It != Connections.end() ? It->second.Stats : ConnectionStats{};
}

std::unordered_map<ConnectionId, ConnectionStats> NetworkManager::GetAllConnectionStats() const
{
    std::lock_guard<std::mutex> Lock(ConnectionsMutex);
    std::unordered_map<ConnectionId, ConnectionStats> Result;
    for (const auto& Pair : Connections)
    {
        Result[Pair.first] = Pair.second.Stats;
    }
    return Result;
}

void NetworkManager::UpdateConfig(const NetworkConfig& Config)
{
    this->Config = Config;
}
NetworkConfig NetworkManager::GetCurrentConfig() const
{
    return Config;
}
std::string NetworkManager::GetConnectionInfo(ConnectionId Id) const
{
    return "Connection " + std::to_string(Id);
}
std::vector<NetworkAddress> NetworkManager::GetLocalAddresses() const
{
    return {};
}
NetworkError NetworkManager::ValidateAddress(const NetworkAddress& Address) const
{
    return NetworkError::SUCCESS;
}

}  // namespace Helianthus::Network