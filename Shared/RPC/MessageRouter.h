#pragma once

#include "Shared/Network/NetworkTypes.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

namespace Helianthus::RPC
{
/**
 * @brief Global message router for simulating network communication in demo
 *
 * This singleton routes messages between client and server instances
 * to simulate real network communication in a single process.
 */
class MessageRouter
{
public:
    struct Message
    {
        Network::ConnectionId FromConnection;
        Network::ConnectionId ToConnection;
        std::string ServerAddress;
        std::vector<char> Data;
        std::chrono::steady_clock::time_point Timestamp;
    };

    using MessageCallback = std::function<void(Network::ConnectionId, const char*, size_t)>;

    static MessageRouter& GetInstance();

    void Initialize();
    void Shutdown();

    // Register server/client instances
    void RegisterServer(const std::string& Address, MessageCallback Callback);
    void UnregisterServer(const std::string& Address);
    void RegisterClient(Network::ConnectionId ClientId, MessageCallback Callback);
    void UnregisterClient(Network::ConnectionId ClientId);

    // Send messages
    void SendToServer(const std::string& ServerAddress,
                      Network::ConnectionId ClientId,
                      const char* Data,
                      size_t Size);
    void SendToClient(Network::ConnectionId ClientId, const char* Data, size_t Size);

    // Connection simulation
    Network::ConnectionId CreateServerConnection(const std::string& ServerAddress);

private:
    MessageRouter() = default;
    ~MessageRouter();

    void ProcessMessages();

    std::mutex ServerMutex;
    std::unordered_map<std::string, MessageCallback> ServerCallbacks;

    std::mutex ClientMutex;
    std::unordered_map<Network::ConnectionId, MessageCallback> ClientCallbacks;

    std::mutex MessageMutex;
    std::condition_variable MessageCondition;
    std::queue<Message> PendingMessages;

    std::thread ProcessingThread;
    std::atomic<bool> ShouldStop{false};
    bool IsInitialized = false;
};

}  // namespace Helianthus::RPC