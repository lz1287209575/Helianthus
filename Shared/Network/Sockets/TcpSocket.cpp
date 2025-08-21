#include "Network/Sockets/TcpSocket.h"

#include "Shared/Common/Types.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include "Network/INetworkSocket.h"
#include "Network/NetworkTypes.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
#include <vector>

#ifdef _WIN32
    #define fcntl ioctlsocket
    #define ssize_t int
    #pragma comment(lib, "Ws2_32.lib")
#endif

#ifndef _WIN32
    #define closesocket close
#endif

namespace Helianthus::Network::Sockets
{
namespace
{
static std::atomic<ConnectionId> NEXT_CONNECTION_ID{1};
}

static NetworkError ConvertErrnoToNetworkError(int Err)
{
    switch (Err)
    {
        case EACCES:
            return NetworkError::PERMISSION_DENIED;
        case EADDRINUSE:
            return NetworkError::BIND_FAILED;
        case ENETUNREACH:
            return NetworkError::NETWORK_UNREACHABLE;
        case ETIMEDOUT:
            return NetworkError::TIMEOUT;
        default:
            return NetworkError::RECEIVE_FAILED;
    }
}

static sockaddr_in MakeSockaddr(const NetworkAddress& Address)
{
    sockaddr_in Addr{};
    Addr.sin_family = AF_INET;
    Addr.sin_port = htons(Address.Port);
    if (::inet_pton(AF_INET, Address.Ip.c_str(), &Addr.sin_addr) != 1)
    {
        Addr.sin_addr.s_addr = INADDR_ANY;
    }
    return Addr;
}

TcpSocket::TcpSocket()
{
    SockImpl = std::make_unique<TcpSocketImpl>();
    SockImpl->Id = NEXT_CONNECTION_ID.fetch_add(1, std::memory_order_relaxed);
}

TcpSocket::~TcpSocket()
{
    Disconnect();
}

NetworkError TcpSocket::Connect(const NetworkAddress& Address)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd != -1)
    {
        return NetworkError::NONE;
    }

#ifdef _WIN32
    SOCKET Fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (Fd == INVALID_SOCKET)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    sockaddr_in Addr = MakeSockaddr(Address);
    if (::connect(Fd, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) == SOCKET_ERROR)
    {
        closesocket(Fd);
        return NetworkError::CONNECTION_FAILED;
    }
#else
    int Fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (Fd < 0)
    {
        return NetworkError::SOCKET_CREATE_FAILED;
    }

    sockaddr_in Addr = MakeSockaddr(Address);
    if (::connect(Fd, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0)
    {
        int Err = errno;
        closesocket(Fd);
        return Err == ETIMEDOUT ? NetworkError::TIMEOUT : NetworkError::CONNECTION_FAILED;
    }
#endif

    SockImpl->Fd = Fd;
    SockImpl->State = ConnectionState::CONNECTED;
    SockImpl->Remote = Address;
    SockImpl->IsServer = false;
    if (SockImpl->OnConnected)
    {
        SockImpl->OnConnected(SockImpl->Id);
    }
    return NetworkError::NONE;
}

NetworkError TcpSocket::Bind(const NetworkAddress& Address)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd == -1)
    {
        SockImpl->Fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (SockImpl->Fd < 0)
        {
            return NetworkError::SOCKET_CREATE_FAILED;
        }
#ifdef _WIN32
        const char* WinOpt = "1";
        ::setsockopt(static_cast<SOCKET>(SockImpl->Fd), SOL_SOCKET, SO_REUSEADDR, WinOpt, sizeof(int));
#else
        int Opt = 1;
        ::setsockopt(SockImpl->Fd, SOL_SOCKET, SO_REUSEADDR, &Opt, sizeof(Opt));
#endif
    }

    sockaddr_in Addr = MakeSockaddr(Address);
#ifdef _WIN32
    if (::bind(static_cast<SOCKET>(SockImpl->Fd), reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) == SOCKET_ERROR)
    {
        return NetworkError::BIND_FAILED;
    }
#else
    if (::bind(SockImpl->Fd, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0)
    {
        return NetworkError::BIND_FAILED;
    }
#endif
    SockImpl->Local = Address;
    return NetworkError::NONE;
}

NetworkError TcpSocket::Listen(uint32_t Backlog)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd < 0)
    {
        return NetworkError::LISTEN_FAILED;
    }
#ifdef _WIN32
    if (::listen(static_cast<SOCKET>(SockImpl->Fd), static_cast<int>(Backlog)) == SOCKET_ERROR)
    {
        return NetworkError::LISTEN_FAILED;
    }
#else
    if (::listen(SockImpl->Fd, static_cast<int>(Backlog)) < 0)
    {
        return NetworkError::LISTEN_FAILED;
    }
#endif
    SockImpl->IsServer = true;
    SockImpl->State = ConnectionState::CONNECTED;
    return NetworkError::NONE;
}

NetworkError TcpSocket::Accept()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd < 0)
    {
        return NetworkError::ACCEPT_FAILED;
    }
    sockaddr_in ClientAddr{};
    socklen_t Len = sizeof(ClientAddr);
#ifdef _WIN32
    SOCKET ClientFd = ::accept(static_cast<SOCKET>(SockImpl->Fd), reinterpret_cast<sockaddr*>(&ClientAddr), &Len);
    if (ClientFd == INVALID_SOCKET)
    {
        return NetworkError::ACCEPT_FAILED;
    }
    closesocket(ClientFd);
#else
    int ClientFd = ::accept(SockImpl->Fd, reinterpret_cast<sockaddr*>(&ClientAddr), &Len);
    if (ClientFd < 0)
    {
        return NetworkError::ACCEPT_FAILED;
    }
    closesocket(ClientFd);
#endif
    return NetworkError::NONE;
}

void TcpSocket::Disconnect()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    SockImpl->StopAsync.store(true);
    if (SockImpl->RecvThread.joinable())
    {
        SockImpl->RecvThread.join();
    }
    // 通知可能存在的 Proactor 取消句柄上的所有挂起 IO（如果上层使用了 IOCP）
    // 当前类不持有 Proactor 指针，取消由上层 Async* 封装或 Manager 负责调用
    if (SockImpl->Fd >= 0)
    {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(SockImpl->Fd));
#else
        closesocket(SockImpl->Fd);
#endif
        SockImpl->Fd = -1;
    }
    if (SockImpl->State == ConnectionState::CONNECTED)
    {
        SockImpl->State = ConnectionState::DISCONNECTED;
        if (SockImpl->OnDisconnected)
        {
            SockImpl->OnDisconnected(SockImpl->Id, NetworkError::NONE);
        }
    }
}

NetworkError TcpSocket::Send(const char* Data, size_t Size, size_t& BytesSent)
{
	BytesSent = 0;
#ifdef _WIN32
    SOCKET FdSnapshot = INVALID_SOCKET;
#else
    int FdSnapshot = -1;
#endif
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (SockImpl->Fd < 0)
		{
			return NetworkError::SEND_FAILED;
		}
#ifdef _WIN32
        FdSnapshot = static_cast<SOCKET>(SockImpl->Fd);
#else
        FdSnapshot = static_cast<int>(SockImpl->Fd);
#endif
	}
	ssize_t N = 0;
#ifdef _WIN32
    N = ::send(FdSnapshot, Data, static_cast<int>(Size), 0);
#else
    N = ::send(FdSnapshot, Data, Size, 0);
#endif
	if (N < 0)
	{
		return ConvertErrnoToNetworkError(errno);
	}
	BytesSent = static_cast<size_t>(N);
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		SockImpl->Stats.BytesSent += static_cast<uint64_t>(BytesSent);
		SockImpl->Stats.PacketsSent += 1;
	}
	return NetworkError::NONE;
}

NetworkError TcpSocket::Receive(char* Buffer, size_t BufferSize, size_t& BytesReceived)
{
	BytesReceived = 0;
#ifdef _WIN32
    SOCKET FdSnapshot = INVALID_SOCKET;
#else
    int FdSnapshot = -1;
#endif
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (SockImpl->Fd < 0)
		{
			return NetworkError::RECEIVE_FAILED;
		}
#ifdef _WIN32
        FdSnapshot = static_cast<SOCKET>(SockImpl->Fd);
#else
        FdSnapshot = static_cast<int>(SockImpl->Fd);
#endif
	}
	int N = 0;
#ifdef _WIN32
    N = ::recv(FdSnapshot, (Buffer), static_cast<int>(BufferSize), 0);
#else
    N = ::recv(FdSnapshot, (Buffer), BufferSize, 0);
#endif
	if (N < 0)
	{
		return ConvertErrnoToNetworkError(errno);
	}
	if (N == 0)
	{
		return NetworkError::CONNECTION_CLOSED;
	}
	BytesReceived = static_cast<size_t>(N);
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		SockImpl->Stats.BytesReceived += static_cast<uint64_t>(BytesReceived);
		SockImpl->Stats.PacketsReceived += 1;
	}
	return NetworkError::NONE;
}

void TcpSocket::StartAsyncReceive()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd < 0 || SockImpl->RecvThread.joinable())
    {
        return;
    }
    SockImpl->StopAsync.store(false);
    SockImpl->RecvThread = std::thread(
        [this]()
        {
            std::vector<char> Buffer(SockImpl->Config.BufferSizeBytes > 0
                                         ? SockImpl->Config.BufferSizeBytes
                                         : HELIANTHUS_DEFAULT_BUFFER_SIZE);
            while (!SockImpl->StopAsync.load())
            {
                int N = ::recv(SockImpl->Fd, Buffer.data(), Buffer.size(), 0);
                if (N > 0)
                {
                    SockImpl->Stats.BytesReceived += static_cast<uint64_t>(N);
                    SockImpl->Stats.PacketsReceived += 1;
                    if (SockImpl->OnDataReceived)
                    {
                        SockImpl->OnDataReceived(
                            SockImpl->Id, Buffer.data(), static_cast<size_t>(N));
                    }
                }
                else if (N == 0)
                {
                    // Peer closed
                    break;
                }
                else
                {
                    int Err = errno;
                    if (Err == EAGAIN || Err == EWOULDBLOCK)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    if (SockImpl->OnError)
                    {
                        SockImpl->OnError(
                            SockImpl->Id, ConvertErrnoToNetworkError(Err), std::strerror(Err));
                    }
                    break;
                }
            }
            // Trigger disconnect callback on exit
            if (SockImpl->OnDisconnected)
            {
                SockImpl->OnDisconnected(SockImpl->Id, NetworkError::NONE);
            }
        });
}

void TcpSocket::StopAsyncReceive()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    SockImpl->StopAsync.store(true);
    if (SockImpl->RecvThread.joinable())
    {
        SockImpl->RecvThread.join();
    }
}

ConnectionState TcpSocket::GetConnectionState() const
{
    return SockImpl->State;
}

NetworkAddress TcpSocket::GetLocalAddress() const
{
    return SockImpl->Local;
}

NetworkAddress TcpSocket::GetRemoteAddress() const
{
    return SockImpl->Remote;
}

ConnectionId TcpSocket::GetConnectionId() const
{
    return SockImpl->Id;
}

ProtocolType TcpSocket::GetProtocolType() const
{
    return ProtocolType::TCP;
}

ConnectionStats TcpSocket::GetConnectionStats() const
{
    return SockImpl->Stats;
}

void TcpSocket::SetSocketOptions(const NetworkConfig& Config)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    SockImpl->Config = Config;
    if (SockImpl->Fd >= 0)
    {
        // 设置 SO_REUSEADDR 选项（跨平台）
        int reuseAddr = Config.ReuseAddr ? 1 : 0;
        if (setsockopt(static_cast<SOCKET>(SockImpl->Fd),
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuseAddr),
                       sizeof(reuseAddr)) < 0)
        {
            // 处理错误
        }

#ifdef _WIN32
        // Windows 特有选项（如 TCP_NODELAY）
        int noDelay = Config.NoDelay ? 1 : 0;
        if (setsockopt(static_cast<SOCKET>(SockImpl->Fd),
                       IPPROTO_TCP,
                       TCP_NODELAY,
                       reinterpret_cast<const char*>(&noDelay),
                       sizeof(noDelay)) < 0)
        {
            // 处理错误
        }
#else
        // Unix 特有选项（如 SO_KEEPALIVE）
        int keepAlive = Config.KeepAlive ? 1 : 0;
        if (setsockopt(SockImpl->Fd,
                       SOL_SOCKET,
                       SO_KEEPALIVE,
                       reinterpret_cast<const char*>(&keepAlive),
                       sizeof(keepAlive)) < 0)
        {
            // 处理错误
        }
#endif
    }

#ifdef _WIN32
    // Windows 特有选项（如 TCP_NODELAY）
    int noDelay = Config.NoDelay ? 1 : 0;
    if (setsockopt(static_cast<SOCKET>(SockImpl->Fd),
                   IPPROTO_TCP,
                   TCP_NODELAY,
                   reinterpret_cast<const char*>(&noDelay),
                   sizeof(noDelay)) < 0)
    {
        // 处理错误
    }
#else
    // Unix 特有选项（如 SO_KEEPALIVE）
    int keepAlive = Config.KeepAlive ? 1 : 0;
    if (setsockopt(SockImpl->Fd,
                   SOL_SOCKET,
                   SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&keepAlive),
                   sizeof(keepAlive)) < 0)
    {
        // 处理错误
    }
#endif
}

NetworkConfig TcpSocket::GetSocketOptions() const
{
    return SockImpl->Config;
}

void TcpSocket::SetOnConnectedCallback(OnConnectedCallback Callback)
{
    SockImpl->OnConnected = std::move(Callback);
}

void TcpSocket::SetOnDisconnectedCallback(OnDisconnectedCallback Callback)
{
    SockImpl->OnDisconnected = std::move(Callback);
}

void TcpSocket::SetOnDataReceivedCallback(OnDataReceivedCallback Callback)
{
    SockImpl->OnDataReceived = std::move(Callback);
}

void TcpSocket::SetOnErrorCallback(OnErrorCallback Callback)
{
    SockImpl->OnError = std::move(Callback);
}

bool TcpSocket::IsConnected() const
{
    return SockImpl->State == ConnectionState::CONNECTED && SockImpl->Fd >= 0;
}

bool TcpSocket::IsListening() const
{
    return SockImpl->IsServer;
}

void TcpSocket::SetBlocking(bool Blocking)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    SockImpl->IsBlocking = Blocking;
    if (SockImpl->Fd >= 0)
    {
#ifdef _WIN32
        u_long mode = Blocking ? 0 : 1;
        ioctlsocket(static_cast<SOCKET>(SockImpl->Fd), FIONBIO, &mode);
#else
        int flags = fcntl(SockImpl->Fd, F_GETFL, 0);
        if (Blocking)
        {
            flags &= ~O_NONBLOCK;
        }
        else
        {
            flags |= O_NONBLOCK;
        }
        fcntl(SockImpl->Fd, F_SETFL, flags);
#endif
    }
}

bool TcpSocket::IsBlocking() const
{
    return SockImpl->IsBlocking;
}

void TcpSocket::UpdatePing()
{
    // Minimal placeholder: application-level ping should be implemented at higher layer
    SockImpl->Stats.PingMs = 0;
}

uint32_t TcpSocket::GetPing() const
{
    return SockImpl->Stats.PingMs;
}

void TcpSocket::ResetStats()
{
    SockImpl->Stats = {};
}

TcpSocket::NativeHandle TcpSocket::GetNativeHandle() const
{
    return static_cast<NativeHandle>(SockImpl->Fd);
}

void TcpSocket::Adopt(NativeHandle Handle,
                      const NetworkAddress& Local,
                      const NetworkAddress& Remote,
                      bool IsServerSide)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (SockImpl->Fd >= 0)
    {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(SockImpl->Fd));
#else
        closesocket(SockImpl->Fd);
#endif
    }
#ifdef _WIN32
    SockImpl->Fd = static_cast<intptr_t>(static_cast<SOCKET>(Handle));
#else
    SockImpl->Fd = static_cast<int>(Handle);
#endif
    SockImpl->Local = Local;
    SockImpl->Remote = Remote;
    SockImpl->IsServer = IsServerSide;
    SockImpl->State = ConnectionState::CONNECTED;
}

}  // namespace Helianthus::Network::Sockets
