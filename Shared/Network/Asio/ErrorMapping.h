#pragma once

#include "Shared/Network/NetworkTypes.h"
#include <errno.h>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace Helianthus::Network::Asio
{
    // 统一的错误映射工具类
    class ErrorMapping
    {
    public:
        // 将系统错误码转换为 NetworkError
        static Network::NetworkError FromSystemError(int SystemError);
        
        // 将 errno 转换为 NetworkError
        static Network::NetworkError FromErrno(int Errno);
        
#ifdef _WIN32
        // 将 WSA 错误码转换为 NetworkError
        static Network::NetworkError FromWsaError(int WsaError);
#endif
        
        // 获取错误描述字符串
        static std::string GetErrorString(Network::NetworkError Error);
        
        // 获取系统错误描述字符串
        static std::string GetSystemErrorString(int SystemError);
        
    private:
        // 内部错误映射表
        static Network::NetworkError MapSystemError(int SystemError);
    };
}
