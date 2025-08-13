#pragma once

#include "NetworkTypes.h"
#include <functional>
#include <vector>

namespace Helianthus::Network
{
    // Callback function types
    using OnConnectedCallback = std::function<void(ConnectionId)>;
    using OnDisconnectedCallback = std::function<void(ConnectionId, NetworkError)>;
    using OnDataReceivedCallback = std::function<void(ConnectionId, const uint8_t* Data, size_t Size)>;
    using OnErrorCallback = std::function<void(ConnectionId, NetworkError, const std::string& Message)>;

    /**
     * @brief Abstract interface for network socket implementations
     * 
     * This interface provides the basic functionality for network communication
     * including TCP, UDP, and future WebSocket support.
     */
    class INetworkSocket
    {
    public:
        virtual ~INetworkSocket() = default;

        // Connection management
        virtual NetworkError Connect(const NetworkAddress& Address) = 0;
        virtual NetworkError Bind(const NetworkAddress& Address) = 0;
        virtual NetworkError Listen(uint32_t Backlog = 128) = 0;
        virtual NetworkError Accept() = 0;
        virtual void Disconnect() = 0;

        // Data transmission
        virtual NetworkError Send(const uint8_t* Data, size_t Size, size_t& BytesSent) = 0;
        virtual NetworkError Receive(uint8_t* Buffer, size_t BufferSize, size_t& BytesReceived) = 0;
        
        // Asynchronous operations
        virtual void StartAsyncReceive() = 0;
        virtual void StopAsyncReceive() = 0;

        // State and information
        virtual ConnectionState GetConnectionState() const = 0;
        virtual NetworkAddress GetLocalAddress() const = 0;
        virtual NetworkAddress GetRemoteAddress() const = 0;
        virtual ConnectionId GetConnectionId() const = 0;
        virtual ProtocolType GetProtocolType() const = 0;
        virtual ConnectionStats GetConnectionStats() const = 0;

        // Configuration
        virtual void SetSocketOptions(const NetworkConfig& Config) = 0;
        virtual NetworkConfig GetSocketOptions() const = 0;

        // Callback registration
        virtual void SetOnConnectedCallback(OnConnectedCallback Callback) = 0;
        virtual void SetOnDisconnectedCallback(OnDisconnectedCallback Callback) = 0;
        virtual void SetOnDataReceivedCallback(OnDataReceivedCallback Callback) = 0;
        virtual void SetOnErrorCallback(OnErrorCallback Callback) = 0;

        // Utility functions
        virtual bool IsConnected() const = 0;
        virtual bool IsListening() const = 0;
        virtual void SetBlocking(bool Blocking) = 0;
        virtual bool IsBlocking() const = 0;

        // Performance monitoring
        virtual void UpdatePing() = 0;
        virtual uint32_t GetPing() const = 0;
        virtual void ResetStats() = 0;
    };

} // namespace Helianthus::Network
