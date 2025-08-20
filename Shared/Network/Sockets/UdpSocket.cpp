#include "Network/Sockets/UdpSocket.h"

#include "Shared/Network/WinSockInit.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "Common/Logger.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

// Platform-safe last socket error
static inline int LastSocketErrorCode()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

namespace Helianthus::Network::Sockets
{
UdpSocket::UdpSocket()
    : SocketHandle(InvalidSocket),
      LocalAddress(),
      RemoteAddress(),
      State(ConnectionState::DISCONNECTED),
      ConnectionIdValue(0),
      IsBlockingFlag(true),
      Config(),
      PacketQueueMutex(),
      IncomingPackets(),
      MaxPacketQueueSize(1000),
      StatsMutex(),
      Stats(),
      PingMs(0),
      OnConnectedHandler(),
      OnDisconnectedHandler(),
      OnDataReceivedHandler(),
      OnErrorHandler()
{
    // Initialize WinSock on Windows
    EnsureWinSockInitialized();
}

UdpSocket::~UdpSocket()
{
    Disconnect();
}

UdpSocket::UdpSocket(UdpSocket&& Other) noexcept
    : SocketHandle(Other.SocketHandle),
      LocalAddress(std::move(Other.LocalAddress)),
      RemoteAddress(std::move(Other.RemoteAddress)),
      State(Other.State),
      ConnectionIdValue(Other.ConnectionIdValue),
      IsBlockingFlag(Other.IsBlockingFlag.load()),
      Config(std::move(Other.Config)),
      PacketQueueMutex(),
      IncomingPackets(std::move(Other.IncomingPackets)),
      MaxPacketQueueSize(Other.MaxPacketQueueSize),
      Stats(Other.Stats),
      StatsMutex(),
      PingMs(Other.PingMs),
      OnConnectedHandler(std::move(Other.OnConnectedHandler)),
      OnDisconnectedHandler(std::move(Other.OnDisconnectedHandler)),
      OnDataReceivedHandler(std::move(Other.OnDataReceivedHandler)),
      OnErrorHandler(std::move(Other.OnErrorHandler))
{
    Other.SocketHandle = InvalidSocket;
    Other.State = ConnectionState::DISCONNECTED;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& Other) noexcept
{
    if (this != &Other)
    {
        Disconnect();

        SocketHandle = Other.SocketHandle;
        LocalAddress = std::move(Other.LocalAddress);
        RemoteAddress = std::move(Other.RemoteAddress);
        State = Other.State;
        ConnectionIdValue = Other.ConnectionIdValue;
        IsBlockingFlag = Other.IsBlockingFlag.load();
        Config = std::move(Other.Config);
        MaxPacketQueueSize = Other.MaxPacketQueueSize;
        IncomingPackets = std::move(Other.IncomingPackets);
        Stats = Other.Stats;
        PingMs = Other.PingMs;
        OnConnectedHandler = std::move(Other.OnConnectedHandler);
        OnDisconnectedHandler = std::move(Other.OnDisconnectedHandler);
        OnDataReceivedHandler = std::move(Other.OnDataReceivedHandler);
        OnErrorHandler = std::move(Other.OnErrorHandler);

        Other.SocketHandle = InvalidSocket;
        Other.State = ConnectionState::DISCONNECTED;
    }
    return *this;
}

NetworkError UdpSocket::Connect(const NetworkAddress& Address)
{
    if (!IsValidSocket())
    {
        NetworkError Error = CreateSocket();
        if (Error != NetworkError::SUCCESS)
        {
            return Error;
        }
    }

    // 允许端口为 0（由系统分配临时端口），只校验 IP 非空
    if (Address.Ip.empty())
    {
        return NetworkError::INVALID_ADDRESS;
    }

    sockaddr_in SockAddr{};
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(Address.Port);

#ifdef _WIN32
    SockAddr.sin_addr.s_addr = inet_addr(Address.Ip.c_str());
#else
    inet_pton(AF_INET, Address.Ip.c_str(), &SockAddr.sin_addr);
#endif

    if (connect(SocketHandle, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) != 0)
    {
        HELIANTHUS_LOG_ERROR("UDP Socket connect failed: " + std::to_string(LastSocketErrorCode()));
        return NetworkError::CONNECTION_FAILED;
    }

    RemoteAddress = Address;
    State = ConnectionState::CONNECTED;

    if (OnConnectedHandler)
    {
        OnConnectedHandler(ConnectionIdValue);
    }

    HELIANTHUS_LOG_INFO("UDP Socket connected to: " + Address.ToString());
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::Bind(const NetworkAddress& Address)
{
    if (!IsValidSocket())
    {
        NetworkError Error = CreateSocket();
        if (Error != NetworkError::SUCCESS)
        {
            return Error;
        }
    }

    if (!Address.IsValid())
    {
        return NetworkError::INVALID_ADDRESS;
    }

    sockaddr_in SockAddr{};
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(Address.Port);

#ifdef _WIN32
    SockAddr.sin_addr.s_addr = inet_addr(Address.Ip.c_str());
#else
    inet_pton(AF_INET, Address.Ip.c_str(), &SockAddr.sin_addr);
#endif

    if (bind(SocketHandle, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) != 0)
    {
        HELIANTHUS_LOG_ERROR("UDP Socket bind failed: " + std::to_string(LastSocketErrorCode()));
        return NetworkError::BIND_FAILED;
    }

    LocalAddress = Address;
    State = ConnectionState::CONNECTED;

    HELIANTHUS_LOG_INFO("UDP Socket bound to: " + Address.ToString());
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::Listen(uint32_t Backlog)
{
    // UDP sockets don't use listen - they're connectionless
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::Accept()
{
    // UDP sockets don't use accept - they're connectionless
    return NetworkError::SUCCESS;
}

void UdpSocket::Disconnect()
{
    if (IsValidSocket())
    {
#ifdef _WIN32
        closesocket(SocketHandle);
#else
        close(SocketHandle);
#endif

        SocketHandle = InvalidSocket;
        State = ConnectionState::DISCONNECTED;

        if (OnDisconnectedHandler)
        {
            OnDisconnectedHandler(ConnectionIdValue, NetworkError::SUCCESS);
        }

        HELIANTHUS_LOG_INFO("UDP Socket disconnected");
    }
}

NetworkError UdpSocket::Send(const char* Data, size_t Size, size_t& BytesSent)
{
    if (!IsValidSocket() || !Data || Size == 0)
    {
        BytesSent = 0;
        return NetworkError::SEND_FAILED;
    }

    if (State != ConnectionState::CONNECTED)
    {
        BytesSent = 0;
        return NetworkError::CONNECTION_CLOSED;
    }

    int Result = send(SocketHandle, Data, static_cast<int>(Size), 0);
    if (Result < 0)
    {
        BytesSent = 0;
        HELIANTHUS_LOG_ERROR("UDP Socket send failed: " + std::to_string(LastSocketErrorCode()));
        return NetworkError::SEND_FAILED;
    }

    BytesSent = static_cast<size_t>(Result);
    UpdateStats(BytesSent, 0);
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::Receive(char* Buffer, size_t BufferSize, size_t& BytesReceived)
{
    if (!IsValidSocket() || !Buffer || BufferSize == 0)
    {
        BytesReceived = 0;
        return NetworkError::RECEIVE_FAILED;
    }

    int Result = recv(SocketHandle, Buffer, static_cast<int>(BufferSize), 0);
    if (Result < 0)
    {
#ifdef _WIN32
        int Error = WSAGetLastError();
        if (Error == WSAEWOULDBLOCK)
        {
            BytesReceived = 0;
            return NetworkError::SUCCESS;  // Non-blocking, no data available
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            BytesReceived = 0;
            return NetworkError::SUCCESS;  // Non-blocking, no data available
        }
#endif

        BytesReceived = 0;
        HELIANTHUS_LOG_ERROR("UDP Socket receive failed: " + std::to_string(LastSocketErrorCode()));
        return NetworkError::RECEIVE_FAILED;
    }

    BytesReceived = static_cast<size_t>(Result);
    UpdateStats(0, BytesReceived);

    if (OnDataReceivedHandler)
    {
        OnDataReceivedHandler(ConnectionIdValue, Buffer, BytesReceived);
    }

    return NetworkError::SUCCESS;
}

void UdpSocket::StartAsyncReceive()
{
    // UDP sockets are inherently async when non-blocking
    SetBlocking(false);
}

void UdpSocket::StopAsyncReceive()
{
    // No specific cleanup needed for UDP
}

ConnectionState UdpSocket::GetConnectionState() const
{
    return State;
}

NetworkAddress UdpSocket::GetLocalAddress() const
{
    return LocalAddress;
}

NetworkAddress UdpSocket::GetRemoteAddress() const
{
    return RemoteAddress;
}

ConnectionId UdpSocket::GetConnectionId() const
{
    return ConnectionIdValue;
}

ProtocolType UdpSocket::GetProtocolType() const
{
    return ProtocolType::UDP;
}

ConnectionStats UdpSocket::GetConnectionStats() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return Stats;
}

UdpSocket::NativeHandle UdpSocket::GetNativeHandle() const
{
    return static_cast<UdpSocket::NativeHandle>(SocketHandle);
}

void UdpSocket::SetSocketOptions(const NetworkConfig& ConfigIn)
{
    Config = ConfigIn;
    if (IsValidSocket())
    {
        ApplySocketOptions();
    }
}

NetworkConfig UdpSocket::GetSocketOptions() const
{
    return Config;
}

void UdpSocket::SetOnConnectedCallback(OnConnectedCallback Callback)
{
    OnConnectedHandler = std::move(Callback);
}

void UdpSocket::SetOnDisconnectedCallback(OnDisconnectedCallback Callback)
{
    OnDisconnectedHandler = std::move(Callback);
}

void UdpSocket::SetOnDataReceivedCallback(OnDataReceivedCallback Callback)
{
    OnDataReceivedHandler = std::move(Callback);
}

void UdpSocket::SetOnErrorCallback(OnErrorCallback Callback)
{
    OnErrorHandler = std::move(Callback);
}

bool UdpSocket::IsConnected() const
{
    return IsValidSocket() && State == ConnectionState::CONNECTED;
}

bool UdpSocket::IsListening() const
{
    // UDP sockets don't listen in the traditional sense
    return IsValidSocket() && State == ConnectionState::CONNECTED;
}

void UdpSocket::SetBlocking(bool Blocking)
{
    if (!IsValidSocket())
    {
        return;
    }

#ifdef _WIN32
    u_long Mode = Blocking ? 0 : 1;
    ioctlsocket(SocketHandle, FIONBIO, &Mode);
#else
    int Flags = fcntl(SocketHandle, F_GETFL, 0);
    if (!Blocking)
    {
        fcntl(SocketHandle, F_SETFL, Flags | O_NONBLOCK);
    }
    else
    {
        fcntl(SocketHandle, F_SETFL, Flags & ~O_NONBLOCK);
    }
#endif

    IsBlockingFlag = Blocking;
}

bool UdpSocket::IsBlocking() const
{
    return IsBlockingFlag.load();
}

void UdpSocket::UpdatePing()
{
    // TODO: Implement ping measurement for UDP
    PingMs = 0;
}

uint32_t UdpSocket::GetPing() const
{
    return PingMs;
}

void UdpSocket::ResetStats()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    Stats = ConnectionStats{};
}

// UDP-specific methods
NetworkError UdpSocket::SendTo(const char* Data, size_t Size, const NetworkAddress& Address)
{
    if (!IsValidSocket() || !Data || Size == 0)
    {
        return NetworkError::SEND_FAILED;
    }

    if (!Address.IsValid())
    {
        return NetworkError::INVALID_ADDRESS;
    }

    sockaddr_in SockAddr{};
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(Address.Port);

#ifdef _WIN32
    SockAddr.sin_addr.s_addr = inet_addr(Address.Ip.c_str());
#else
    inet_pton(AF_INET, Address.Ip.c_str(), &SockAddr.sin_addr);
#endif

    int Result = sendto(SocketHandle,
                        Data,
                        static_cast<int>(Size),
                        0,
                        reinterpret_cast<sockaddr*>(&SockAddr),
                        sizeof(SockAddr));
    if (Result < 0)
    {
        HELIANTHUS_LOG_ERROR("UDP Socket sendto failed: " + std::to_string(LastSocketErrorCode()));
        return NetworkError::SEND_FAILED;
    }

    UpdateStats(Result, 0);
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::ReceiveFrom(char* Buffer,
                                    size_t BufferSize,
                                    size_t& BytesReceived,
                                    NetworkAddress& FromAddress)
{
    if (!IsValidSocket() || !Buffer || BufferSize == 0)
    {
        BytesReceived = 0;
        return NetworkError::RECEIVE_FAILED;
    }

    sockaddr_in SockAddr{};
    socklen_t SockAddrLen = sizeof(SockAddr);

    int Result = recvfrom(SocketHandle,
                          Buffer,
                          static_cast<int>(BufferSize),
                          0,
                          reinterpret_cast<sockaddr*>(&SockAddr),
                          &SockAddrLen);
    if (Result < 0)
    {
#ifdef _WIN32
        int Error = WSAGetLastError();
        if (Error == WSAEWOULDBLOCK)
        {
            BytesReceived = 0;
            return NetworkError::SUCCESS;  // Non-blocking, no data available
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            BytesReceived = 0;
            return NetworkError::SUCCESS;  // Non-blocking, no data available
        }
#endif

        BytesReceived = 0;
        HELIANTHUS_LOG_ERROR("UDP Socket recvfrom failed: " +
                             std::to_string(LastSocketErrorCode()));
        return NetworkError::RECEIVE_FAILED;
    }

    BytesReceived = static_cast<size_t>(Result);

    // Extract sender address
    char IpStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &SockAddr.sin_addr, IpStr, INET_ADDRSTRLEN);
    FromAddress = NetworkAddress(IpStr, ntohs(SockAddr.sin_port));

    UpdateStats(0, BytesReceived);
    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::EnableBroadcast(bool Enable)
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    int Option = Enable ? 1 : 0;
    if (setsockopt(SocketHandle,
                   SOL_SOCKET,
                   SO_BROADCAST,
                   reinterpret_cast<char*>(&Option),
                   sizeof(Option)) != 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::JoinMulticastGroup(const std::string& MulticastAddress)
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    struct ip_mreq Mreq;
    Mreq.imr_multiaddr.s_addr = inet_addr(MulticastAddress.c_str());
    Mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(SocketHandle,
                   IPPROTO_IP,
                   IP_ADD_MEMBERSHIP,
                   reinterpret_cast<char*>(&Mreq),
                   sizeof(Mreq)) != 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::LeaveMulticastGroup(const std::string& MulticastAddress)
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    struct ip_mreq Mreq;
    Mreq.imr_multiaddr.s_addr = inet_addr(MulticastAddress.c_str());
    Mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(SocketHandle,
                   IPPROTO_IP,
                   IP_DROP_MEMBERSHIP,
                   reinterpret_cast<char*>(&Mreq),
                   sizeof(Mreq)) != 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::SetMulticastTTL(uint8_t TTL)
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    if (setsockopt(SocketHandle,
                   IPPROTO_IP,
                   IP_MULTICAST_TTL,
                   reinterpret_cast<char*>(&TTL),
                   sizeof(TTL)) != 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return NetworkError::SUCCESS;
}

NetworkError UdpSocket::SetMulticastLoopback(bool Enable)
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    int Option = Enable ? 1 : 0;
    if (setsockopt(SocketHandle,
                   IPPROTO_IP,
                   IP_MULTICAST_LOOP,
                   reinterpret_cast<char*>(&Option),
                   sizeof(Option)) != 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return NetworkError::SUCCESS;
}

bool UdpSocket::HasIncomingPackets() const
{
    std::lock_guard<std::mutex> Lock(PacketQueueMutex);
    return !IncomingPackets.empty();
}

UdpSocket::Packet UdpSocket::GetNextPacket()
{
    std::lock_guard<std::mutex> Lock(PacketQueueMutex);

    if (IncomingPackets.empty())
    {
        return Packet{};
    }

    Packet PacketValue = std::move(IncomingPackets.front());
    IncomingPackets.pop();
    return PacketValue;
}

std::vector<UdpSocket::Packet> UdpSocket::GetAllPackets()
{
    std::lock_guard<std::mutex> Lock(PacketQueueMutex);

    std::vector<Packet> Packets;
    Packets.reserve(IncomingPackets.size());

    while (!IncomingPackets.empty())
    {
        Packets.push_back(std::move(IncomingPackets.front()));
        IncomingPackets.pop();
    }

    return Packets;
}

uint32_t UdpSocket::GetIncomingPacketCount() const
{
    std::lock_guard<std::mutex> Lock(PacketQueueMutex);
    return static_cast<uint32_t>(IncomingPackets.size());
}

// Private helper methods
NetworkError UdpSocket::CreateSocket()
{
    SocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!IsValidSocket())
    {
        HELIANTHUS_LOG_ERROR("Failed to create UDP socket: " +
                             std::to_string(LastSocketErrorCode()));
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    return ApplySocketOptions();
}

NetworkError UdpSocket::ApplySocketOptions()
{
    if (!IsValidSocket())
    {
        return NetworkError::NOT_INITIALIZED;
    }

    // Set reuse address
    int ReuseAddr = 1;
    if (setsockopt(SocketHandle,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   reinterpret_cast<char*>(&ReuseAddr),
                   sizeof(ReuseAddr)) != 0)
    {
        HELIANTHUS_LOG_WARN("Failed to set SO_REUSEADDR on UDP socket");
    }

    // Set timeout if configured
    if (Config.ConnectionTimeoutMs > 0)
    {
#ifdef _WIN32
        DWORD Timeout = static_cast<DWORD>(Config.ConnectionTimeoutMs);
        setsockopt(SocketHandle,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   reinterpret_cast<char*>(&Timeout),
                   sizeof(Timeout));
        setsockopt(SocketHandle,
                   SOL_SOCKET,
                   SO_SNDTIMEO,
                   reinterpret_cast<char*>(&Timeout),
                   sizeof(Timeout));
#else
        struct timeval Timeout;
        Timeout.tv_sec = Config.ConnectionTimeoutMs / 1000;
        Timeout.tv_usec = (Config.ConnectionTimeoutMs % 1000) * 1000;
        setsockopt(SocketHandle, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof(Timeout));
        setsockopt(SocketHandle, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof(Timeout));
#endif
    }

    return NetworkError::SUCCESS;
}

void UdpSocket::UpdateStats(uint64_t BytesSent, uint64_t BytesReceived)
{
    std::lock_guard<std::mutex> Lock(StatsMutex);

    if (BytesSent > 0)
    {
        Stats.BytesSent += BytesSent;
        Stats.PacketsSent++;
    }

    if (BytesReceived > 0)
    {
        Stats.BytesReceived += BytesReceived;
        Stats.PacketsReceived++;
    }
}

void UdpSocket::AddIncomingPacket(const char* Data, size_t Size, const NetworkAddress& FromAddress)
{
    std::lock_guard<std::mutex> Lock(PacketQueueMutex);

    if (IncomingPackets.size() >= MaxPacketQueueSize)
    {
        // Remove oldest packet if queue is full
        IncomingPackets.pop();
    }

    Packet PacketValue;
    PacketValue.Data.assign(Data, Data + Size);
    PacketValue.FromAddress = FromAddress;
    PacketValue.Timestamp = GetCurrentTimestampMs();
    PacketValue.SequenceNumber = 0;  // TODO: Implement sequence numbering

    IncomingPackets.push(std::move(PacketValue));
}

bool UdpSocket::IsValidSocket() const
{
#ifdef _WIN32
    return SocketHandle != INVALID_SOCKET;
#else
    return SocketHandle >= 0;
#endif
}

Common::TimestampMs UdpSocket::GetCurrentTimestampMs() const
{
    auto Now = std::chrono::high_resolution_clock::now();
    auto Duration = Now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count();
}

}  // namespace Helianthus::Network::Sockets
