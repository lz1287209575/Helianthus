#include "Shared/Network/Asio/ErrorMapping.h"

#include <cstring>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

namespace Helianthus::Network::Asio
{
Network::NetworkError ErrorMapping::FromSystemError(int SystemError)
{
    return MapSystemError(SystemError);
}

Network::NetworkError ErrorMapping::FromErrno(int Errno)
{
    return MapSystemError(Errno);
}

#ifdef _WIN32
Network::NetworkError ErrorMapping::FromWsaError(int WsaError)
{
    switch (WsaError)
    {
        case WSAEINTR:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAEBADF:
            return Network::NetworkError::SOCKET_CREATE_FAILED;
        case WSAEACCES:
            return Network::NetworkError::PERMISSION_DENIED;
        case WSAEFAULT:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case WSAEINVAL:
            return Network::NetworkError::INVALID_ADDRESS;
        case WSAEMFILE:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEWOULDBLOCK:
            return Network::NetworkError::TIMEOUT;
        case WSAEINPROGRESS:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEALREADY:
            return Network::NetworkError::ALREADY_INITIALIZED;
        case WSAENOTSOCK:
            return Network::NetworkError::SOCKET_CREATE_FAILED;
        case WSAEDESTADDRREQ:
            return Network::NetworkError::INVALID_ADDRESS;
        case WSAEMSGSIZE:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case WSAEPROTOTYPE:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAENOPROTOOPT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEPROTONOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAESOCKTNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEOPNOTSUPP:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEPFNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEAFNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEADDRINUSE:
            return Network::NetworkError::BIND_FAILED;
        case WSAEADDRNOTAVAIL:
            return Network::NetworkError::INVALID_ADDRESS;
        case WSAENETDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAENETUNREACH:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAENETRESET:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAECONNABORTED:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAECONNRESET:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAENOBUFS:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case WSAEISCONN:
            return Network::NetworkError::ALREADY_INITIALIZED;
        case WSAENOTCONN:
            return Network::NetworkError::CONNECTION_NOT_FOUND;
        case WSAESHUTDOWN:
            return Network::NetworkError::CONNECTION_CLOSED;
        case WSAETOOMANYREFS:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAETIMEDOUT:
            return Network::NetworkError::TIMEOUT;
        case WSAECONNREFUSED:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAELOOP:
            return Network::NetworkError::INVALID_ADDRESS;
        case WSAENAMETOOLONG:
            return Network::NetworkError::INVALID_ADDRESS;
        case WSAEHOSTDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAEHOSTUNREACH:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case WSAENOTEMPTY:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEPROCLIM:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEUSERS:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEDQUOT:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAESTALE:
            return Network::NetworkError::CONNECTION_FAILED;
        case WSAEREMOTE:
            return Network::NetworkError::CONNECTION_FAILED;
        default:
            return Network::NetworkError::CONNECTION_FAILED;
    }
}
#endif

std::string ErrorMapping::GetErrorString(Network::NetworkError Error)
{
    switch (Error)
    {
        case Network::NetworkError::NONE:
            return "Success";
        case Network::NetworkError::CONNECTION_FAILED:
            return "Connection failed";
        case Network::NetworkError::SOCKET_CREATE_FAILED:
            return "Socket creation failed";
        case Network::NetworkError::BIND_FAILED:
            return "Bind failed";
        case Network::NetworkError::LISTEN_FAILED:
            return "Listen failed";
        case Network::NetworkError::ACCEPT_FAILED:
            return "Accept failed";
        case Network::NetworkError::SEND_FAILED:
            return "Send failed";
        case Network::NetworkError::RECEIVE_FAILED:
            return "Receive failed";
        case Network::NetworkError::TIMEOUT:
            return "Operation timeout";
        case Network::NetworkError::BUFFER_OVERFLOW:
            return "Buffer overflow";
        case Network::NetworkError::INVALID_ADDRESS:
            return "Invalid address";
        case Network::NetworkError::PERMISSION_DENIED:
            return "Permission denied";
        case Network::NetworkError::NETWORK_UNREACHABLE:
            return "Network unreachable";
        case Network::NetworkError::ALREADY_INITIALIZED:
            return "Already initialized";
        case Network::NetworkError::NOT_INITIALIZED:
            return "Not initialized";
        case Network::NetworkError::CONNECTION_NOT_FOUND:
            return "Connection not found";
        case Network::NetworkError::CONNECTION_CLOSED:
            return "Connection closed";
        case Network::NetworkError::SERIALIZATION_FAILED:
            return "Serialization failed";
        case Network::NetworkError::GROUP_NOT_FOUND:
            return "Group not found";
        case Network::NetworkError::SERVER_ALREADY_RUNNING:
            return "Server already running";
        default:
            return "Unknown error";
    }
}

std::string ErrorMapping::GetSystemErrorString(int SystemError)
{
#ifdef _WIN32
    char* MsgBuf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr,
                   SystemError,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<LPSTR>(&MsgBuf),
                   0,
                   nullptr);

    if (MsgBuf != nullptr)
    {
        std::string Result(MsgBuf);
        LocalFree(MsgBuf);
        return Result;
    }
    else
    {
        return "Unknown system error: " + std::to_string(SystemError);
    }
#else
    return std::string(strerror(SystemError));
#endif
}

Network::NetworkError ErrorMapping::MapSystemError(int SystemError)
{
#ifdef _WIN32
    return FromWsaError(SystemError);
#else
    // 简化错误映射，避免重复的 case 值
    if (SystemError == 0)
    {
        return Network::NetworkError::NONE;
    }

    // 常见的 POSIX 错误码映射
    switch (SystemError)
    {
        case EINTR:
            return Network::NetworkError::CONNECTION_CLOSED;
        case EBADF:
            return Network::NetworkError::SOCKET_CREATE_FAILED;
        case EACCES:
            return Network::NetworkError::PERMISSION_DENIED;
        case EFAULT:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case EINVAL:
            return Network::NetworkError::INVALID_ADDRESS;
        case EMFILE:
            return Network::NetworkError::CONNECTION_FAILED;
        case EINPROGRESS:
            return Network::NetworkError::CONNECTION_FAILED;
        case EALREADY:
            return Network::NetworkError::ALREADY_INITIALIZED;
        case ENOTSOCK:
            return Network::NetworkError::SOCKET_CREATE_FAILED;
        case EDESTADDRREQ:
            return Network::NetworkError::INVALID_ADDRESS;
        case EMSGSIZE:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case EPROTOTYPE:
            return Network::NetworkError::CONNECTION_FAILED;
        case ENOPROTOOPT:
            return Network::NetworkError::CONNECTION_FAILED;
        case EPROTONOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case ESOCKTNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case EOPNOTSUPP:
            return Network::NetworkError::CONNECTION_FAILED;
        case EPFNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case EAFNOSUPPORT:
            return Network::NetworkError::CONNECTION_FAILED;
        case EADDRINUSE:
            return Network::NetworkError::BIND_FAILED;
        case EADDRNOTAVAIL:
            return Network::NetworkError::INVALID_ADDRESS;
        case ENETDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case ENETUNREACH:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case ENETRESET:
            return Network::NetworkError::CONNECTION_CLOSED;
        case ECONNABORTED:
            return Network::NetworkError::CONNECTION_CLOSED;
        case ECONNRESET:
            return Network::NetworkError::CONNECTION_CLOSED;
        case ENOBUFS:
            return Network::NetworkError::BUFFER_OVERFLOW;
        case EISCONN:
            return Network::NetworkError::ALREADY_INITIALIZED;
        case ENOTCONN:
            return Network::NetworkError::CONNECTION_NOT_FOUND;
        case ESHUTDOWN:
            return Network::NetworkError::CONNECTION_CLOSED;
        case ETOOMANYREFS:
            return Network::NetworkError::CONNECTION_FAILED;
        case ETIMEDOUT:
            return Network::NetworkError::TIMEOUT;
        case ECONNREFUSED:
            return Network::NetworkError::CONNECTION_FAILED;
        case ELOOP:
            return Network::NetworkError::INVALID_ADDRESS;
        case ENAMETOOLONG:
            return Network::NetworkError::INVALID_ADDRESS;
        case EHOSTDOWN:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case EHOSTUNREACH:
            return Network::NetworkError::NETWORK_UNREACHABLE;
        case ENOTEMPTY:
            return Network::NetworkError::CONNECTION_FAILED;
        case EUSERS:
            return Network::NetworkError::CONNECTION_FAILED;
        case EDQUOT:
            return Network::NetworkError::CONNECTION_FAILED;
        case ESTALE:
            return Network::NetworkError::CONNECTION_FAILED;
        case EREMOTE:
            return Network::NetworkError::CONNECTION_FAILED;
        default:
            // 处理 EAGAIN/EWOULDBLOCK 等可能重复的错误码
            if (SystemError == EAGAIN || SystemError == EWOULDBLOCK)
            {
                return Network::NetworkError::TIMEOUT;
            }
            return Network::NetworkError::CONNECTION_FAILED;
    }
#endif
}
}  // namespace Helianthus::Network::Asio
