#include "Shared/Network/Asio/IAsyncSocket.h"
#include "Shared/Network/Asio/AsyncTcpSocket.h"
#include "Shared/Network/Asio/AsyncUdpSocket.h"
#include "Shared/Network/Asio/IoContext.h"
#include "Common/Logger.h"
#include "Common/LogCategories.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace Helianthus::Network::Asio;
using namespace Helianthus::Network;

class UnifiedApiExample
{
private:
    std::atomic<bool> TcpCompleted{false};
    std::atomic<bool> UdpCompleted{false};
    std::atomic<bool> TimeoutCompleted{false};

public:
    void RunTcpExample()
    {
        std::cout << "=== TCP 统一API示例 ===" << std::endl;
        
        // 创建IoContext
        auto ContextPtr = std::make_shared<IoContext>();
        
        // 在后台线程运行IoContext
        std::thread ContextThread([ContextPtr]() {
            ContextPtr->Run();
        });
        
        // 创建TCP Socket
        auto TcpSocket = std::make_unique<AsyncTcpSocket>(ContextPtr);
        
        // 设置默认超时
        TcpSocket->SetDefaultTimeout(2000); // 2秒超时
        
        // 创建取消令牌
        auto CancelToken = CreateCancelToken();
        
        // 异步连接 - 连接到不存在的地址以演示错误处理
        TcpSocket->AsyncConnect(NetworkAddress("127.0.0.1", 9999), 
            [this, &TcpSocket, CancelToken](NetworkError Error) {
                if (Error != NetworkError::NONE)
                {
                    std::cout << "TCP连接失败（预期）: " << static_cast<int>(Error) << std::endl;
                    TcpCompleted = true;
                    return;
                }
                
                std::cout << "TCP连接成功" << std::endl;
                
                // 发送数据
                const char* Data = "Hello from unified API!";
                TcpSocket->AsyncSend(Data, strlen(Data), NetworkAddress{},
                    [this](NetworkError Error, size_t Bytes) {
                        if (Error != NetworkError::NONE)
                        {
                            std::cout << "TCP发送失败: " << static_cast<int>(Error) << std::endl;
                        }
                        else
                        {
                            std::cout << "TCP发送成功: " << Bytes << " 字节" << std::endl;
                        }
                        TcpCompleted = true;
                    }, CancelToken, 1000); // 1秒超时
            }, CancelToken, 3000); // 3秒连接超时
        
        // 等待操作完成或超时
        auto StartTime = std::chrono::steady_clock::now();
        while (!TcpCompleted && 
               std::chrono::steady_clock::now() - StartTime < std::chrono::seconds(5))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!TcpCompleted)
        {
            std::cout << "TCP操作超时，强制取消..." << std::endl;
            CancelOperation(CancelToken);
        }
        
        // 停止IoContext
        ContextPtr->Stop();
        if (ContextThread.joinable())
        {
            ContextThread.join();
        }
        
        std::cout << "TCP示例完成" << std::endl;
    }
    
    void RunUdpExample()
    {
        std::cout << "\n=== UDP 统一API示例 ===" << std::endl;
        
        // 创建IoContext
        auto ContextPtr = std::make_shared<IoContext>();
        
        // 在后台线程运行IoContext
        std::thread ContextThread([ContextPtr]() {
            ContextPtr->Run();
        });
        
        // 创建UDP Socket
        auto UdpSocket = std::make_unique<AsyncUdpSocket>(ContextPtr);
        
        // 绑定本地地址
        auto BindResult = UdpSocket->Bind(NetworkAddress("127.0.0.1", 0));
        if (BindResult != NetworkError::NONE)
        {
            std::cout << "UDP绑定失败: " << static_cast<int>(BindResult) << std::endl;
            ContextPtr->Stop();
            if (ContextThread.joinable())
            {
                ContextThread.join();
            }
            return;
        }
        
        std::cout << "UDP绑定成功，本地地址: " << UdpSocket->GetLocalAddress().ToString() << std::endl;
        
        // 创建取消令牌
        auto CancelToken = CreateCancelToken();
        
        // 发送数据到本地回环地址
        const char* Data = "Hello UDP from unified API!";
        UdpSocket->AsyncSend(Data, strlen(Data), NetworkAddress("127.0.0.1", 8081),
            [this](NetworkError Error, size_t Bytes) {
                if (Error != NetworkError::NONE)
                {
                    std::cout << "UDP发送失败: " << static_cast<int>(Error) << std::endl;
                }
                else
                {
                    std::cout << "UDP发送成功: " << Bytes << " 字节" << std::endl;
                }
                UdpCompleted = true;
            }, CancelToken, 1000); // 1秒超时
        
        // 等待操作完成或超时
        auto StartTime = std::chrono::steady_clock::now();
        while (!UdpCompleted && 
               std::chrono::steady_clock::now() - StartTime < std::chrono::seconds(3))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!UdpCompleted)
        {
            std::cout << "UDP操作超时，强制取消..." << std::endl;
            CancelOperation(CancelToken);
        }
        
        // 停止IoContext
        ContextPtr->Stop();
        if (ContextThread.joinable())
        {
            ContextThread.join();
        }
        
        std::cout << "UDP示例完成" << std::endl;
    }
    
    void RunTimeoutExample()
    {
        std::cout << "\n=== 超时示例 ===" << std::endl;
        
        auto ContextPtr = std::make_shared<IoContext>();
        
        // 在后台线程运行IoContext
        std::thread ContextThread([ContextPtr]() {
            ContextPtr->Run();
        });
        
        auto TcpSocket = std::make_unique<AsyncTcpSocket>(ContextPtr);
        
        // 设置很短的超时时间
        TcpSocket->SetDefaultTimeout(100); // 100ms超时
        
        // 尝试连接到不存在的地址
        TcpSocket->AsyncConnect(NetworkAddress("192.168.1.999", 9999),
            [this](NetworkError Error) {
                if (Error == NetworkError::TIMEOUT)
                {
                    std::cout << "连接超时，符合预期" << std::endl;
                }
                else
                {
                    std::cout << "连接结果: " << static_cast<int>(Error) << std::endl;
                }
                TimeoutCompleted = true;
            }, nullptr, 500); // 500ms超时
        
        // 等待操作完成或超时
        auto StartTime = std::chrono::steady_clock::now();
        while (!TimeoutCompleted && 
               std::chrono::steady_clock::now() - StartTime < std::chrono::seconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 停止IoContext
        ContextPtr->Stop();
        if (ContextThread.joinable())
        {
            ContextThread.join();
        }
        
        std::cout << "超时示例完成" << std::endl;
    }
    
    void Run()
    {
        std::cout << "开始统一API示例..." << std::endl;
        
        RunTcpExample();
        RunUdpExample();
        RunTimeoutExample();
        
        std::cout << "\n统一API示例完成" << std::endl;
    }
};

int main()
{
    UnifiedApiExample Example;
    Example.Run();
    return 0;
}
