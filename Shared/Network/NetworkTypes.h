#pragma once

#include "HelianthusConfig.h"
#include <cstdint>
#include <string>
#include <memory>

namespace Helianthus::Network
{
    // Network connection state enumeration
    enum class ConnectionState : uint8_t
    {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        DISCONNECTING = 3,
        ERROR_STATE = 4
    };

    // Network protocol types
    enum class ProtocolType : uint8_t
    {
        TCP = 0,
        UDP = 1,
        WEBSOCKET = 2
    };

    // Network error codes
    enum class NetworkError : int32_t
    {
        NONE = 0,
        CONNECTION_FAILED = -1,
        SOCKET_CREATE_FAILED = -2,
        BIND_FAILED = -3,
        LISTEN_FAILED = -4,
        ACCEPT_FAILED = -5,
        SEND_FAILED = -6,
        RECEIVE_FAILED = -7,
        TIMEOUT = -8,
        BUFFER_OVERFLOW = -9,
        INVALID_ADDRESS = -10,
        PERMISSION_DENIED = -11,
        NETWORK_UNREACHABLE = -12
    };

    // Network address structure
    struct NetworkAddress
    {
        std::string Ip;
        uint16_t Port;
        
        NetworkAddress() : Port(0) {}
        NetworkAddress(const std::string& Ip, uint16_t Port) : Ip(Ip), Port(Port) {}
        
        bool IsValid() const
        {
            return !Ip.empty() && Port > 0;
        }
        
        std::string ToString() const
        {
            return Ip + ":" + std::to_string(Port);
        }
    };

    // Connection statistics
    struct ConnectionStats
    {
        uint64_t BytesSent = 0;
        uint64_t BytesReceived = 0;
        uint64_t PacketsSent = 0;
        uint64_t PacketsReceived = 0;
        uint64_t ConnectionTimeMs = 0;
        uint32_t PingMs = 0;
    };

    // Network configuration
    struct NetworkConfig
    {
        uint32_t MaxConnections = HELIANTHUS_MAX_CONNECTIONS;
        uint32_t BufferSize = HELIANTHUS_DEFAULT_BUFFER_SIZE;
        uint32_t TimeoutMs = HELIANTHUS_NETWORK_TIMEOUT_MS;
        uint32_t ThreadPoolSize = HELIANTHUS_DEFAULT_THREAD_POOL_SIZE;
        bool EnableNagle = false;
        bool EnableKeepalive = true;
        uint32_t KeepaliveIntervalMs = 30000;
    };

    // Forward declarations
    class INetworkSocket;
    class INetworkManager;
    class NetworkBuffer;

    // Smart pointer type definitions
    using NetworkSocketPtr = std::shared_ptr<INetworkSocket>;
    using NetworkManagerPtr = std::shared_ptr<INetworkManager>;
    using NetworkBufferPtr = std::shared_ptr<NetworkBuffer>;

    // Connection ID type
    using ConnectionId = uint64_t;
    static constexpr ConnectionId InvalidConnectionId = 0;

} // namespace Helianthus::Network
