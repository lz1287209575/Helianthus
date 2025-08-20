#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

#include "Message/Message.h"
#include "Message/MessageQueue.h"
#include "Network/NetworkTypes.h"
#include "Network/Sockets/TcpSocket.h"

namespace Helianthus::Network
{
using MessageHandler = std::function<void(const Message::Message&)>;
using ConnectionHandler = std::function<void(ConnectionId, NetworkError)>;

/**
 * @brief Core network manager that handles connections and message routing
 *
 * Manages TCP connections, message queues, and provides high-level networking API.
 * Thread-safe and supports connection pooling with automatic cleanup.
 */
class NetworkManager
{
public:
    NetworkManager();
    ~NetworkManager();

    // Disable copy, allow move
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    NetworkManager(NetworkManager&& Other) noexcept;
    NetworkManager& operator=(NetworkManager&& Other) noexcept;

    // Core lifecycle
    NetworkError Initialize(const NetworkConfig& Config);
    void Shutdown();
    bool IsInitialized() const;

    // Connection management
    NetworkError CreateConnection(const NetworkAddress& Address, ConnectionId& OutConnectionId);
    NetworkError CloseConnection(ConnectionId Id);
    NetworkError CloseAllConnections();
    bool IsConnectionActive(ConnectionId Id) const;
    ConnectionState GetConnectionState(ConnectionId Id) const;
    std::vector<ConnectionId> GetActiveConnections() const;

    // Server functionality
    NetworkError StartServer(const NetworkAddress& BindAddress);
    NetworkError StopServer();
    bool IsServerRunning() const;

    // Message operations
    NetworkError SendMessage(ConnectionId Id, const Message::Message& Msg);
    NetworkError SendMessageReliable(ConnectionId Id, const Message::Message& Msg);
    NetworkError BroadcastMessage(const Message::Message& Msg);
    NetworkError BroadcastMessageToGroup(const std::string& GroupName, const Message::Message& Msg);

    // Message queue operations
    bool HasIncomingMessages() const;
    Message::MessagePtr GetNextMessage();
    std::vector<Message::MessagePtr> GetAllMessages();
    uint32_t GetIncomingMessageCount() const;

    // Connection grouping
    NetworkError AddConnectionToGroup(ConnectionId Id, const std::string& GroupName);
    NetworkError RemoveConnectionFromGroup(ConnectionId Id, const std::string& GroupName);
    std::vector<ConnectionId> GetConnectionsInGroup(const std::string& GroupName) const;
    void ClearGroup(const std::string& GroupName);

    // Event handlers
    void SetMessageHandler(MessageHandler Handler);
    void SetConnectionHandler(ConnectionHandler Handler);
    void RemoveAllHandlers();

    // Statistics and monitoring
    NetworkStats GetNetworkStats() const;
    ConnectionStats GetConnectionStats(ConnectionId Id) const;
    std::unordered_map<ConnectionId, ConnectionStats> GetAllConnectionStats() const;

    // Configuration
    void UpdateConfig(const NetworkConfig& Config);
    NetworkConfig GetCurrentConfig() const;

    // Utility methods
    std::string GetConnectionInfo(ConnectionId Id) const;
    std::vector<NetworkAddress> GetLocalAddresses() const;
    NetworkError ValidateAddress(const NetworkAddress& Address) const;

private:
    struct ConnectionEntry
    {
        std::unique_ptr<Sockets::TcpSocket> Socket;
        NetworkAddress RemoteAddress;
        ConnectionState State;
        Common::TimestampMs CreationTime;
        Common::TimestampMs LastActivity;
        std::set<std::string> Groups;
        ConnectionStats Stats;

        bool IsActive() const
        {
            return State == ConnectionState::CONNECTED;
        }
    };

    // Helper methods
    ConnectionId GenerateConnectionId();
    void CleanupConnection(ConnectionId Id);
    void ProcessIncomingData(ConnectionId Id, const std::vector<char>& Data);
    void HandleConnectionStateChange(ConnectionId Id, ConnectionState NewState);
    void UpdateConnectionActivity(ConnectionId Id);
    void NotifyMessageReceived(const Message::Message& Msg);
    void NotifyConnectionStateChange(ConnectionId Id, NetworkError Error);
    void StartMessageProcessingThread();
    void StopMessageProcessingThread();
    void MessageProcessingLoop();
    void StartServerAcceptThread();
    void StopServerAcceptThread();
    void ServerAcceptLoop();
    NetworkError AcceptConnection();

private:
    // Configuration and state
    NetworkConfig Config;
    std::atomic<bool> InitializedFlag = false;
    std::atomic<bool> ShuttingDownFlag = false;

    // Connection management
    mutable std::mutex ConnectionsMutex;
    std::unordered_map<ConnectionId, ConnectionEntry> Connections;
    std::atomic<ConnectionId> NextConnectionId = 1;

    // Connection grouping
    mutable std::mutex GroupsMutex;
    std::unordered_map<std::string, std::set<ConnectionId>> ConnectionGroups;

    // Server state
    std::atomic<bool> ServerRunningFlag = false;
    std::unique_ptr<Sockets::TcpSocket> ServerSocket;
    std::thread ServerAcceptThread;

    // Message processing
    std::unique_ptr<Message::MessageQueue> IncomingMessages;
    std::unique_ptr<Message::MessageQueue> OutgoingMessages;
    std::thread MessageProcessingThread;
    std::atomic<bool> StopMessageProcessing = false;

    // Event handlers
    mutable std::mutex HandlersMutex;
    MessageHandler MessageHandlerFunc;
    ConnectionHandler ConnectionHandlerFunc;

    // Statistics
    mutable std::mutex StatsMutex;
    NetworkStats Stats;

    // Thread management
    std::atomic<bool> StopServerAccept = false;
};

}  // namespace Helianthus::Network