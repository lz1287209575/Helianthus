#include "Network/Sockets/UdpSocket.h"
#include "Common/Logger.h"
#include <cstring>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace Helianthus::Network::Sockets
{
    UdpSocket::UdpSocket()
        : SocketHandle(InvalidSocket)
        , State(ConnectionState::DISCONNECTED)
        , ConnectionId(0)
        , IsBlocking(true)
        , MaxPacketQueueSize(1000)
        , PingMs(0)
    {
        Stats = ConnectionStats{};
        Config = NetworkConfig{};
    }

    UdpSocket::~UdpSocket()
    {
        Disconnect();
    }

    UdpSocket::UdpSocket(UdpSocket&& Other) noexcept
        : SocketHandle(Other.SocketHandle)
        , LocalAddress_(std::move(Other.LocalAddress_))
        , RemoteAddress_(std::move(Other.RemoteAddress_))
        , State(Other.State)
        , ConnectionId(Other.ConnectionId)
        , IsBlocking(Other.IsBlocking.load())
        , Config(std::move(Other.Config))
        , MaxPacketQueueSize(Other.MaxPacketQueueSize)
        , IncomingPackets(std::move(Other.IncomingPackets))
        , Stats(Other.Stats)
        , PingMs(Other.PingMs)
        , OnConnectedCallback_(std::move(Other.OnConnectedCallback_))
        , OnDisconnectedCallback_(std::move(Other.OnDisconnectedCallback_))
        , OnDataReceivedCallback_(std::move(Other.OnDataReceivedCallback_))
        , OnErrorCallback_(std::move(Other.OnErrorCallback_))
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
            LocalAddress_ = std::move(Other.LocalAddress_);
            RemoteAddress_ = std::move(Other.RemoteAddress_);
            State = Other.State;
            ConnectionId = Other.ConnectionId;
            IsBlocking = Other.IsBlocking.load();
            Config = std::move(Other.Config);
            MaxPacketQueueSize = Other.MaxPacketQueueSize;
            IncomingPackets = std::move(Other.IncomingPackets);
            Stats = Other.Stats;
            PingMs = Other.PingMs;
            OnConnectedCallback_ = std::move(Other.OnConnectedCallback_);
            OnDisconnectedCallback_ = std::move(Other.OnDisconnectedCallback_);
            OnDataReceivedCallback_ = std::move(Other.OnDataReceivedCallback_);
            OnErrorCallback_ = std::move(Other.OnErrorCallback_);

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

        if (connect(SocketHandle, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) != 0)
        {
            HELIANTHUS_LOG_ERROR("UDP Socket connect failed: " + std::to_string(GetLastError()));
            return NetworkError::CONNECTION_FAILED;
        }

        RemoteAddress_ = Address;
        State = ConnectionState::CONNECTED;
        
        if (OnConnectedCallback_)
        {
            OnConnectedCallback_(ConnectionId);
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
            HELIANTHUS_LOG_ERROR("UDP Socket bind failed: " + std::to_string(GetLastError()));
            return NetworkError::BIND_FAILED;
        }

        LocalAddress_ = Address;
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
            
            if (OnDisconnectedCallback_)
            {
                OnDisconnectedCallback_(ConnectionId, NetworkError::SUCCESS);
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
            HELIANTHUS_LOG_ERROR("UDP Socket send failed: " + std::to_string(GetLastError()));
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
                    return NetworkError::SUCCESS; // Non-blocking, no data available
                }
            #else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    BytesReceived = 0;
                    return NetworkError::SUCCESS; // Non-blocking, no data available
                }
            #endif
            
            BytesReceived = 0;
            HELIANTHUS_LOG_ERROR("UDP Socket receive failed: " + std::to_string(GetLastError()));
            return NetworkError::RECEIVE_FAILED;
        }

        BytesReceived = static_cast<size_t>(Result);
        UpdateStats(0, BytesReceived);
        
        if (OnDataReceivedCallback_)
        {
            OnDataReceivedCallback_(ConnectionId, Buffer, BytesReceived);
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
        return LocalAddress_;
    }

    NetworkAddress UdpSocket::GetRemoteAddress() const
    {
        return RemoteAddress_;
    }

    ConnectionId UdpSocket::GetConnectionId() const
    {
        return ConnectionId;
    }

    ProtocolType UdpSocket::GetProtocolType() const
    {
        return ProtocolType::UDP;
    }

    ConnectionStats UdpSocket::GetConnectionStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex_);
        return Stats;
    }

    void UdpSocket::SetSocketOptions(const NetworkConfig& Config)
    {
        Config = Config;
        if (IsValidSocket())
        {
            SetSocketOptions();
        }
    }

    NetworkConfig UdpSocket::GetSocketOptions() const
    {
        return Config;
    }

    void UdpSocket::SetOnConnectedCallback(OnConnectedCallback Callback)
    {
        OnConnectedCallback_ = std::move(Callback);
    }

    void UdpSocket::SetOnDisconnectedCallback(OnDisconnectedCallback Callback)
    {
        OnDisconnectedCallback_ = std::move(Callback);
    }

    void UdpSocket::SetOnDataReceivedCallback(OnDataReceivedCallback Callback)
    {
        OnDataReceivedCallback_ = std::move(Callback);
    }

    void UdpSocket::SetOnErrorCallback(OnErrorCallback Callback)
    {
        OnErrorCallback_ = std::move(Callback);
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

        IsBlocking = Blocking;
    }

    bool UdpSocket::IsBlocking() const
    {
        return IsBlocking.load();
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
        std::lock_guard<std::mutex> Lock(StatsMutex_);
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

        int Result = sendto(SocketHandle, Data, static_cast<int>(Size), 0, 
                          reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr));
        if (Result < 0)
        {
            HELIANTHUS_LOG_ERROR("UDP Socket sendto failed: " + std::to_string(GetLastError()));
            return NetworkError::SEND_FAILED;
        }

        UpdateStats(Result, 0);
        return NetworkError::SUCCESS;
    }

    NetworkError UdpSocket::ReceiveFrom(char* Buffer, size_t BufferSize, size_t& BytesReceived, NetworkAddress& FromAddress)
    {
        if (!IsValidSocket() || !Buffer || BufferSize == 0)
        {
            BytesReceived = 0;
            return NetworkError::RECEIVE_FAILED;
        }

        sockaddr_in SockAddr{};
        socklen_t SockAddrLen = sizeof(SockAddr);

        int Result = recvfrom(SocketHandle, Buffer, static_cast<int>(BufferSize), 0,
                             reinterpret_cast<sockaddr*>(&SockAddr), &SockAddrLen);
        if (Result < 0)
        {
            #ifdef _WIN32
                int Error = WSAGetLastError();
                if (Error == WSAEWOULDBLOCK)
                {
                    BytesReceived = 0;
                    return NetworkError::SUCCESS; // Non-blocking, no data available
                }
            #else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    BytesReceived = 0;
                    return NetworkError::SUCCESS; // Non-blocking, no data available
                }
            #endif
            
            BytesReceived = 0;
            HELIANTHUS_LOG_ERROR("UDP Socket recvfrom failed: " + std::to_string(GetLastError()));
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
        if (setsockopt(SocketHandle, SOL_SOCKET, SO_BROADCAST, 
                      reinterpret_cast<char*>(&Option), sizeof(Option)) != 0)
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

        if (setsockopt(SocketHandle, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                      reinterpret_cast<char*>(&Mreq), sizeof(Mreq)) != 0)
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

        if (setsockopt(SocketHandle, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
                      reinterpret_cast<char*>(&Mreq), sizeof(Mreq)) != 0)
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

        if (setsockopt(SocketHandle, IPPROTO_IP, IP_MULTICAST_TTL, 
                      reinterpret_cast<char*>(&TTL), sizeof(TTL)) != 0)
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
        if (setsockopt(SocketHandle, IPPROTO_IP, IP_MULTICAST_LOOP, 
                      reinterpret_cast<char*>(&Option), sizeof(Option)) != 0)
        {
            return NetworkError::SOCKET_CREATE_FAILED;
        }

        return NetworkError::SUCCESS;
    }

    bool UdpSocket::HasIncomingPackets() const
    {
        std::lock_guard<std::mutex> Lock(PacketQueueMutex_);
        return !IncomingPackets.empty();
    }

    UdpSocket::Packet UdpSocket::GetNextPacket()
    {
        std::lock_guard<std::mutex> Lock(PacketQueueMutex_);
        
        if (IncomingPackets.empty())
        {
            return Packet{};
        }

        Packet Packet = std::move(IncomingPackets.front());
        IncomingPackets.pop();
        return Packet;
    }

    std::vector<UdpSocket::Packet> UdpSocket::GetAllPackets()
    {
        std::lock_guard<std::mutex> Lock(PacketQueueMutex_);
        
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
        std::lock_guard<std::mutex> Lock(PacketQueueMutex_);
        return static_cast<uint32_t>(IncomingPackets.size());
    }

    // Private helper methods
    NetworkError UdpSocket::CreateSocket()
    {
        SocketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!IsValidSocket())
        {
            HELIANTHUS_LOG_ERROR("Failed to create UDP socket: " + std::to_string(GetLastError()));
            return NetworkError::SOCKET_CREATE_FAILED;
        }

        return SetSocketOptions();
    }

    NetworkError UdpSocket::SetSocketOptions()
    {
        if (!IsValidSocket())
        {
            return NetworkError::NOT_INITIALIZED;
        }

        // Set reuse address
        int ReuseAddr = 1;
        if (setsockopt(SocketHandle, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<char*>(&ReuseAddr), sizeof(ReuseAddr)) != 0)
        {
            HELIANTHUS_LOG_WARN("Failed to set SO_REUSEADDR on UDP socket");
        }

        // Set timeout if configured
        if (Config.ConnectionTimeoutMs > 0)
        {
            #ifdef _WIN32
                DWORD Timeout = static_cast<DWORD>(Config.ConnectionTimeoutMs);
                setsockopt(SocketHandle, SOL_SOCKET, SO_RCVTIMEO, 
                          reinterpret_cast<char*>(&Timeout), sizeof(Timeout));
                setsockopt(SocketHandle, SOL_SOCKET, SO_SNDTIMEO, 
                          reinterpret_cast<char*>(&Timeout), sizeof(Timeout));
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
        std::lock_guard<std::mutex> Lock(StatsMutex_);
        
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
        std::lock_guard<std::mutex> Lock(PacketQueueMutex_);
        
        if (IncomingPackets.size() >= MaxPacketQueueSize)
        {
            // Remove oldest packet if queue is full
            IncomingPackets.pop();
        }
        
        Packet Packet;
        Packet.Data.assign(Data, Data + Size);
        Packet.FromAddress = FromAddress;
        Packet.Timestamp = GetCurrentTimestampMs();
        Packet.SequenceNumber = 0; // TODO: Implement sequence numbering
        
        IncomingPackets.push(std::move(Packet));
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

} // namespace Helianthus::Network::Sockets
