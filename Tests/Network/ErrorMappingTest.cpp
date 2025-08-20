#include "Shared/Network/Asio/ErrorMapping.h"

#include <errno.h>
#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class ErrorMappingTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};

TEST_F(ErrorMappingTest, BasicErrorMapping)
{
    // 测试基本的错误映射
    EXPECT_EQ(ErrorMapping::FromErrno(0), NetworkError::NONE);
    EXPECT_EQ(ErrorMapping::FromErrno(EINTR), NetworkError::CONNECTION_CLOSED);
    EXPECT_EQ(ErrorMapping::FromErrno(EBADF), NetworkError::SOCKET_CREATE_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EACCES), NetworkError::PERMISSION_DENIED);
    EXPECT_EQ(ErrorMapping::FromErrno(EFAULT), NetworkError::BUFFER_OVERFLOW);
    EXPECT_EQ(ErrorMapping::FromErrno(EINVAL), NetworkError::INVALID_ADDRESS);
    EXPECT_EQ(ErrorMapping::FromErrno(EMFILE), NetworkError::CONNECTION_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EINPROGRESS), NetworkError::CONNECTION_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EALREADY), NetworkError::ALREADY_INITIALIZED);
    EXPECT_EQ(ErrorMapping::FromErrno(ENOTSOCK), NetworkError::SOCKET_CREATE_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EDESTADDRREQ), NetworkError::INVALID_ADDRESS);
    EXPECT_EQ(ErrorMapping::FromErrno(EMSGSIZE), NetworkError::BUFFER_OVERFLOW);
    EXPECT_EQ(ErrorMapping::FromErrno(EADDRINUSE), NetworkError::BIND_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EADDRNOTAVAIL), NetworkError::INVALID_ADDRESS);
    EXPECT_EQ(ErrorMapping::FromErrno(ENETDOWN), NetworkError::NETWORK_UNREACHABLE);
    EXPECT_EQ(ErrorMapping::FromErrno(ENETUNREACH), NetworkError::NETWORK_UNREACHABLE);
    EXPECT_EQ(ErrorMapping::FromErrno(ENETRESET), NetworkError::CONNECTION_CLOSED);
    EXPECT_EQ(ErrorMapping::FromErrno(ECONNABORTED), NetworkError::CONNECTION_CLOSED);
    EXPECT_EQ(ErrorMapping::FromErrno(ECONNRESET), NetworkError::CONNECTION_CLOSED);
    EXPECT_EQ(ErrorMapping::FromErrno(ENOBUFS), NetworkError::BUFFER_OVERFLOW);
    EXPECT_EQ(ErrorMapping::FromErrno(EISCONN), NetworkError::ALREADY_INITIALIZED);
    EXPECT_EQ(ErrorMapping::FromErrno(ENOTCONN), NetworkError::CONNECTION_NOT_FOUND);
    EXPECT_EQ(ErrorMapping::FromErrno(ESHUTDOWN), NetworkError::CONNECTION_CLOSED);
    EXPECT_EQ(ErrorMapping::FromErrno(ETIMEDOUT), NetworkError::TIMEOUT);
    EXPECT_EQ(ErrorMapping::FromErrno(ECONNREFUSED), NetworkError::CONNECTION_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(EHOSTDOWN), NetworkError::NETWORK_UNREACHABLE);
    EXPECT_EQ(ErrorMapping::FromErrno(EHOSTUNREACH), NetworkError::NETWORK_UNREACHABLE);
}

TEST_F(ErrorMappingTest, TimeoutErrorMapping)
{
    // 测试超时相关的错误映射
    // EAGAIN 和 EWOULDBLOCK 在某些系统上可能相同
    if (EAGAIN != EWOULDBLOCK)
    {
        EXPECT_EQ(ErrorMapping::FromErrno(EAGAIN), NetworkError::TIMEOUT);
    }
    EXPECT_EQ(ErrorMapping::FromErrno(EWOULDBLOCK), NetworkError::TIMEOUT);
    EXPECT_EQ(ErrorMapping::FromErrno(ETIMEDOUT), NetworkError::TIMEOUT);
}

TEST_F(ErrorMappingTest, UnknownErrorMapping)
{
    // 测试未知错误码的映射
    EXPECT_EQ(ErrorMapping::FromErrno(99999), NetworkError::CONNECTION_FAILED);
    EXPECT_EQ(ErrorMapping::FromErrno(-1), NetworkError::CONNECTION_FAILED);
}

TEST_F(ErrorMappingTest, ErrorStringConversion)
{
    // 测试错误字符串转换
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::NONE), "Success");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::CONNECTION_FAILED), "Connection failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::SOCKET_CREATE_FAILED),
              "Socket creation failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::BIND_FAILED), "Bind failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::LISTEN_FAILED), "Listen failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::ACCEPT_FAILED), "Accept failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::SEND_FAILED), "Send failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::RECEIVE_FAILED), "Receive failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::TIMEOUT), "Operation timeout");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::BUFFER_OVERFLOW), "Buffer overflow");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::INVALID_ADDRESS), "Invalid address");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::PERMISSION_DENIED), "Permission denied");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::NETWORK_UNREACHABLE),
              "Network unreachable");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::ALREADY_INITIALIZED),
              "Already initialized");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::NOT_INITIALIZED), "Not initialized");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::CONNECTION_NOT_FOUND),
              "Connection not found");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::CONNECTION_CLOSED), "Connection closed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::SERIALIZATION_FAILED),
              "Serialization failed");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::GROUP_NOT_FOUND), "Group not found");
    EXPECT_EQ(ErrorMapping::GetErrorString(NetworkError::SERVER_ALREADY_RUNNING),
              "Server already running");
}

TEST_F(ErrorMappingTest, SystemErrorString)
{
    // 测试系统错误字符串获取
    std::string ErrorStr = ErrorMapping::GetSystemErrorString(EINVAL);
    EXPECT_FALSE(ErrorStr.empty());
    EXPECT_NE(ErrorStr, "Unknown system error: 22");

    // 测试未知错误码
    std::string UnknownErrorStr = ErrorMapping::GetSystemErrorString(99999);
    EXPECT_FALSE(UnknownErrorStr.empty());
}

TEST_F(ErrorMappingTest, FromSystemError)
{
    // 测试 FromSystemError 方法
    EXPECT_EQ(ErrorMapping::FromSystemError(0), NetworkError::NONE);
    EXPECT_EQ(ErrorMapping::FromSystemError(EINVAL), NetworkError::INVALID_ADDRESS);
    EXPECT_EQ(ErrorMapping::FromSystemError(ECONNREFUSED), NetworkError::CONNECTION_FAILED);
}
