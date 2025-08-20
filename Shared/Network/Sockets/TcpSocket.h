#pragma once

#include "Shared/Network/NetworkTypes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "Network/INetworkSocket.h"
#include "Network/NetworkTypes.h"

namespace Helianthus::Network::Sockets
{
struct TcpSocketImpl
{
    intptr_t Fd = -1;
    bool IsServer = false;
    bool IsBlocking = true;
    std::atomic<bool> StopAsync{false};
    std::thread RecvThread;
    ConnectionState State = ConnectionState::DISCONNECTED;
    NetworkAddress Local;
    NetworkAddress Remote;
    ConnectionStats Stats;
    NetworkConfig Config;
    ConnectionId Id = 0;

    OnConnectedCallback OnConnected;
    OnDisconnectedCallback OnDisconnected;
    OnDataReceivedCallback OnDataReceived;
    OnErrorCallback OnError;
};

class TcpSocket : public INetworkSocket
{
public:
    TcpSocket();
    virtual ~TcpSocket();

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

    // Native handle for reactor integration (cross-platform uintptr_t)
    using NativeHandle = uintptr_t;
    NativeHandle GetNativeHandle() const;

    // Adopt an existing native handle (e.g., from AcceptEx)
    void Adopt(NativeHandle Handle,
               const NetworkAddress& Local,
               const NetworkAddress& Remote,
               bool IsServerSide);

protected:
    std::unique_ptr<TcpSocketImpl> SockImpl;
    std::mutex Mutex;
    std::atomic<bool> IsAsyncReceiving{false};
};
}  // namespace Helianthus::Network::Sockets
