#pragma once

#include "Network/NetworkTypes.h"
#include "Network/Sockets/TcpSocket.h"
#include "Message/Message.h"
#include "Message/MessageQueue.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <set>

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
        void ProcessIncomingData(ConnectionId Id, const std::vector<uint8_t>& Data);
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
        NetworkConfig Config_;
        std::atomic<bool> IsInitialized_ = false;
        std::atomic<bool> IsShuttingDown_ = false;

        // Connection management
        mutable std::mutex ConnectionsMutex_;
        std::unordered_map<ConnectionId, ConnectionEntry> Connections_;
        std::atomic<ConnectionId> NextConnectionId_ = 1;

        // Connection grouping
        mutable std::mutex GroupsMutex_;
        std::unordered_map<std::string, std::set<ConnectionId>> ConnectionGroups_;

        // Server state
        std::atomic<bool> IsServerRunning_ = false;
        std::unique_ptr<Sockets::TcpSocket> ServerSocket_;
        std::thread ServerAcceptThread_;

        // Message processing
        std::unique_ptr<Message::MessageQueue> IncomingMessages_;
        std::unique_ptr<Message::MessageQueue> OutgoingMessages_;
        std::thread MessageProcessingThread_;
        std::atomic<bool> StopMessageProcessing_ = false;

        // Event handlers
        mutable std::mutex HandlersMutex_;
        MessageHandler MessageHandler_;
        ConnectionHandler ConnectionHandler_;

        // Statistics
        mutable std::mutex StatsMutex_;
        NetworkStats Stats_;

        // Thread management
        std::atomic<bool> StopServerAccept_ = false;
    };

} // namespace Helianthus::Network