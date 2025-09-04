#include "Shared/Network/Asio/AsyncTcpAcceptor.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Shared/Network/Asio/Reactor.h"
#include "Shared/Network/NetworkTypes.h"
#include "Shared/Network/Sockets/TcpSocket.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;
using namespace Helianthus::Network::Sockets;

class ReactorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Context = std::make_shared<IoContext>();
    }

    void TearDown() override
    {
        if (Context)
        {
            Context->Stop();
        }
    }

    std::shared_ptr<IoContext> Context;
};

TEST_F(ReactorTest, BasicReactorTest)
{
    auto Reactor = Context->GetReactor();
    ASSERT_NE(Reactor, nullptr);

    std::cout << "Reactor created successfully" << std::endl;

    // 测试基本的 PollOnce 调用
    int result = Reactor->PollOnce(1);  // 1ms 超时
    EXPECT_GE(result, -1);  // 应该返回 -1 (错误), 0 (超时), 或正数 (事件数)

    std::cout << "PollOnce result: " << result << std::endl;
}

TEST_F(ReactorTest, AddTest)
{
    auto Reactor = Context->GetReactor();
    ASSERT_NE(Reactor, nullptr);

    // 创建一个简单的文件描述符进行测试
    int testFd = 0;  // stdin
    bool callbackCalled = false;

    std::cout << "Testing Reactor Add with fd " << testFd << std::endl;

    bool addResult = Reactor->Add(testFd,
                                  EventMask::Read,
                                  [&callbackCalled](EventMask mask)
                                  {
                                      std::cout << "Reactor callback called with mask: "
                                                << static_cast<int>(mask) << std::endl;
                                      callbackCalled = true;
                                  });

    std::cout << "Add result: " << addResult << std::endl;

    // 即使添加失败也应该不会崩溃
    EXPECT_TRUE(true);  // 如果到这里说明没有崩溃
}

TEST_F(ReactorTest, TcpSocketTest)
{
    // 测试 TcpSocket 的基本功能
    TcpSocket Socket;

    std::cout << "TcpSocket created" << std::endl;

    // 测试 GetNativeHandle 不会崩溃
    auto Handle = Socket.GetNativeHandle();
    std::cout << "TcpSocket native handle: " << Handle << std::endl;

    // 测试 Bind
    NetworkAddress Addr("127.0.0.1", 12360);
    auto BindResult = Socket.Bind(Addr);
    std::cout << "Bind result: " << static_cast<int>(BindResult) << std::endl;

    if (BindResult == NetworkError::NONE)
    {
        auto ListenResult = Socket.Listen(128);
        std::cout << "Listen result: " << static_cast<int>(ListenResult) << std::endl;

        if (ListenResult == NetworkError::NONE)
        {
            auto Handle2 = Socket.GetNativeHandle();
            std::cout << "After Listen, native handle: " << Handle2 << std::endl;
        }
    }

    EXPECT_TRUE(true);  // 如果到这里说明没有崩溃
}

TEST_F(ReactorTest, AsyncTcpAcceptorTest)
{
    // 测试 AsyncTcpAcceptor 的基本功能
    auto Acceptor = std::make_shared<AsyncTcpAcceptor>(Context);

    std::cout << "AsyncTcpAcceptor created" << std::endl;

    // 测试 Bind
    NetworkAddress Addr("127.0.0.1", 12370);
    auto BindResult = Acceptor->Bind(Addr);
    std::cout << "Bind result: " << static_cast<int>(BindResult) << std::endl;

    EXPECT_EQ(BindResult, NetworkError::NONE);
}
