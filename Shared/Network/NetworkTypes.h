#pragma once
#ifndef HELIANTHUS_NETWORK_NETWORKTYPES_H
#define HELIANTHUS_NETWORK_NETWORKTYPES_H

#include "HelianthusConfig.h"
#include <cstdint>
#include <string>
#include <memory>

namespace Helianthus::Network
{
    // Network connection state enumeration
    enum class ConnectionState
    {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        DISCONNECTING = 3,
        ERROR_STATE = 4
    };

    // Network protocol types
    enum class ProtocolType
    {
        TCP = 0,
        UDP = 1,
        WEBSOCKET = 2
    };

    // Network error codes
    enum class NetworkError
    {
        SUCCESS = 0,
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
        NETWORK_UNREACHABLE = -12,
        ALREADY_INITIALIZED = -13,
        NOT_INITIALIZED = -14,
        CONNECTION_NOT_FOUND = -15,
        CONNECTION_CLOSED = -16,
        SERIALIZATION_FAILED = -17,
        GROUP_NOT_FOUND = -18,
        SERVER_ALREADY_RUNNING = -19
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

    // Network statistics
    struct NetworkStats
    {
        uint64_t TotalConnectionsCreated = 0;
        uint64_t TotalConnectionsClosed = 0;
        uint32_t ActiveConnections = 0;
        uint64_t TotalMessagesSent = 0;
        uint64_t TotalMessagesReceived = 0;
        uint64_t TotalBytesSent = 0;
        uint64_t TotalBytesReceived = 0;
        uint64_t AverageLatencyMs = 0;
        uint64_t MaxLatencyMs = 0;
    };

    // Network configuration
    struct NetworkConfig
    {
        uint32_t MaxConnections = HELIANTHUS_MAX_CONNECTIONS;
        uint32_t BufferSizeBytes = HELIANTHUS_DEFAULT_BUFFER_SIZE;
        bool NoDelay = false; // 禁用 Nagle 算法
        bool ReuseAddr = false; // 允许地址复用
        bool KeepAlive = false; // 启用 TCP 保活机制
        uint32_t ConnectionTimeoutMs = HELIANTHUS_NETWORK_TIMEOUT_MS;
        uint32_t KeepAliveIntervalMs = 30000;
        uint32_t ThreadPoolSize = HELIANTHUS_DEFAULT_THREAD_POOL_SIZE;
        bool EnableNagle = false;
        bool EnableKeepalive = true;
        bool EnableCompression = false;
        bool EnableEncryption = false;
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

#endif // HELIANTHUS_NETWORK_NETWORKTYPES_H
