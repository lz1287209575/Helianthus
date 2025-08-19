#pragma once

#include "Network/INetworkSocket.h"
#include "Network/NetworkTypes.h"
#include "Common/Logger.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <queue>
#include <cstdint>

namespace Helianthus::Network::Sockets
{
    /**
     * @brief UDP Socket implementation for connectionless communication
     * 
     * Provides UDP socket functionality with support for:
     * - Connectionless data transmission
     * - Broadcast and multicast support
     * - Non-blocking I/O operations
     * - Packet fragmentation and reassembly
     */
    class UdpSocket : public INetworkSocket
    {
    public:
        UdpSocket();
        virtual ~UdpSocket();

        // Disable copy, allow move
        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;
        UdpSocket(UdpSocket&& Other) noexcept;
        UdpSocket& operator=(UdpSocket&& Other) noexcept;

        // INetworkSocket interface
        NetworkError Connect(const NetworkAddress& Address) override;
        NetworkError Bind(const NetworkAddress& Address) override;
        NetworkError Listen(uint32_t Backlog = 128) override;
        NetworkError Accept() override;
        void Disconnect() override;

        NetworkError Send(const char* Data, size_t Size, size_t& BytesSent) override;
        NetworkError Receive(char* Buffer, size_t BufferSize, size_t& BytesReceived) override;
        
        void StartAsyncReceive() override;
        void StopAsyncReceive() override;

        ConnectionState GetConnectionState() const override;
        NetworkAddress GetLocalAddress() const override;
        NetworkAddress GetRemoteAddress() const override;
        ConnectionId GetConnectionId() const override;
        ProtocolType GetProtocolType() const override;
        ConnectionStats GetConnectionStats() const override;

        void SetSocketOptions(const NetworkConfig& Config) override;
        NetworkConfig GetSocketOptions() const override;

        void SetOnConnectedCallback(OnConnectedCallback Callback) override;
        void SetOnDisconnectedCallback(OnDisconnectedCallback Callback) override;
        void SetOnDataReceivedCallback(OnDataReceivedCallback Callback) override;
        void SetOnErrorCallback(OnErrorCallback Callback) override;

        bool IsConnected() const override;
        bool IsListening() const override;
        void SetBlocking(bool Blocking) override;
        bool IsBlocking() const override;

        void UpdatePing() override;
        uint32_t GetPing() const override;
        void ResetStats() override;

        // Native handle access (for integration with reactor/event loop)
        // 使用 uintptr_t 作为跨平台句柄表示（Windows SOCKET/Posix fd 都可安全转换）
        using NativeHandle = uintptr_t;
        NativeHandle GetNativeHandle() const;

        // UDP-specific methods
        NetworkError SendTo(const char* Data, size_t Size, const NetworkAddress& Address);
        NetworkError ReceiveFrom(char* Buffer, size_t BufferSize, size_t& BytesReceived, NetworkAddress& FromAddress);
        
        // Broadcast and multicast support
        NetworkError EnableBroadcast(bool Enable);
        NetworkError JoinMulticastGroup(const std::string& MulticastAddress);
        NetworkError LeaveMulticastGroup(const std::string& MulticastAddress);
        NetworkError SetMulticastTTL(uint8_t TTL);
        NetworkError SetMulticastLoopback(bool Enable);

        // Packet management
        struct Packet
        {
            std::vector<char> Data;
            NetworkAddress FromAddress;
            Common::TimestampMs Timestamp;
            uint32_t SequenceNumber;
        };

        bool HasIncomingPackets() const;
        Packet GetNextPacket();
        std::vector<Packet> GetAllPackets();
        uint32_t GetIncomingPacketCount() const;

    private:
        // Platform-specific socket handle
        #ifdef _WIN32
            using NativeSocketHandle = SOCKET;
            static constexpr NativeSocketHandle InvalidSocket = INVALID_SOCKET;
        #else
            using NativeSocketHandle = int;
            static constexpr NativeSocketHandle InvalidSocket = -1;
        #endif

        NativeSocketHandle SocketHandle;
        NetworkAddress LocalAddress;
        NetworkAddress RemoteAddress;
        ConnectionState State;
        ConnectionId ConnectionIdValue;
        std::atomic<bool> IsBlockingFlag;
        NetworkConfig Config;
        
        // Packet queue for incoming data
        mutable std::mutex PacketQueueMutex;
        std::queue<Packet> IncomingPackets;
        uint32_t MaxPacketQueueSize;

        // Statistics
        mutable std::mutex StatsMutex;
        ConnectionStats Stats;
        uint32_t PingMs;

        // Callbacks
        OnConnectedCallback OnConnectedHandler;
        OnDisconnectedCallback OnDisconnectedHandler;
        OnDataReceivedCallback OnDataReceivedHandler;
        OnErrorCallback OnErrorHandler;

        // Helper methods
        NetworkError CreateSocket();
        NetworkError ApplySocketOptions();
        void UpdateStats(uint64_t BytesSent, uint64_t BytesReceived);
        void AddIncomingPacket(const char* Data, size_t Size, const NetworkAddress& FromAddress);
        bool IsValidSocket() const;
        Common::TimestampMs GetCurrentTimestampMs() const;
    };

} // namespace Helianthus::Network::Sockets
