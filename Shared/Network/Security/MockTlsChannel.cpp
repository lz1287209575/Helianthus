#include "Shared/Network/Security/MockTlsChannel.h"
#include "Shared/Network/NetworkTypes.h"
#include "Common/Logger.h"
#include "Common/LogCategories.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace Helianthus::Network::Security
{
    MockTlsChannel::MockTlsChannel()
        : HandshakeState(TlsHandshakeState::INITIAL)
        , SocketFd(0)
        , IsInitialized(false)
        , IsShutdown(false)
    {
    }

    MockTlsChannel::~MockTlsChannel()
    {
        Shutdown();
    }

    NetworkError MockTlsChannel::Initialize(const TlsConfig& ConfigIn)
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (IsInitialized)
        {
            return NetworkError::ALREADY_INITIALIZED;
        }

        Config = ConfigIn;
        
        // 模拟初始化过程
        std::cout << "MockTlsChannel: 初始化TLS通道" << std::endl;
        std::cout << "  - 证书文件: " << (Config.CertificateFile.empty() ? "无" : Config.CertificateFile) << std::endl;
        std::cout << "  - 私钥文件: " << (Config.PrivateKeyFile.empty() ? "无" : Config.PrivateKeyFile) << std::endl;
        std::cout << "  - CA证书文件: " << (Config.CaCertificateFile.empty() ? "无" : Config.CaCertificateFile) << std::endl;
        std::cout << "  - 验证对端: " << (Config.VerifyPeer ? "是" : "否") << std::endl;
        std::cout << "  - 握手超时: " << Config.HandshakeTimeoutMs << "ms" << std::endl;

        IsInitialized = true;
        HandshakeState = TlsHandshakeState::INITIAL;
        
        return NetworkError::NONE;
    }

    void MockTlsChannel::Shutdown()
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (IsShutdown)
        {
            return;
        }

        std::cout << "MockTlsChannel: 关闭TLS通道" << std::endl;

        // 清除回调
        ClearCallbacks();

        IsShutdown = true;
        IsInitialized = false;
        HandshakeState = TlsHandshakeState::CLOSED;
    }

    void MockTlsChannel::AsyncHandshake(TlsHandshakeHandler Handler)
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || IsShutdown)
        {
            if (Handler) Handler(NetworkError::NOT_INITIALIZED, TlsHandshakeState::FAILED);
            return;
        }

        if (HandshakeState == TlsHandshakeState::CONNECTED)
        {
            if (Handler) Handler(NetworkError::NONE, TlsHandshakeState::CONNECTED);
            return;
        }

        HandshakeHandler = std::move(Handler);
        HandshakeState = TlsHandshakeState::CONNECTING;

        std::cout << "MockTlsChannel: 开始TLS握手..." << std::endl;

        // 模拟异步握手过程
        std::thread([this]() {
            // 模拟握手延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            std::lock_guard<std::mutex> Lock(StateMutex);
            
            if (IsShutdown)
            {
                return;
            }

            // 模拟握手成功
            HandshakeState = TlsHandshakeState::CONNECTED;
            std::cout << "MockTlsChannel: TLS握手成功" << std::endl;
            
            auto Handler = std::move(HandshakeHandler);
            if (Handler) Handler(NetworkError::NONE, TlsHandshakeState::CONNECTED);
        }).detach();
    }

    TlsHandshakeState MockTlsChannel::GetHandshakeState() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        return HandshakeState;
    }

    void MockTlsChannel::AsyncRead(char* Buffer, size_t BufferSize, TlsReadHandler Handler)
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || IsShutdown)
        {
            if (Handler) Handler(NetworkError::NOT_INITIALIZED, 0);
            return;
        }

        if (HandshakeState != TlsHandshakeState::CONNECTED)
        {
            if (Handler) Handler(NetworkError::CONNECTION_FAILED, 0);
            return;
        }

        ReadHandler = std::move(Handler);

        std::cout << "MockTlsChannel: 开始异步读取，缓冲区大小: " << BufferSize << std::endl;

        // 模拟异步读取过程
        std::thread([this, Buffer, BufferSize]() {
            // 模拟读取延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::lock_guard<std::mutex> Lock(StateMutex);
            
            if (IsShutdown)
            {
                return;
            }

            // 模拟读取成功
            const char* MockData = "Mock TLS encrypted data";
            size_t DataSize = std::min(strlen(MockData), BufferSize);
            memcpy(Buffer, MockData, DataSize);
            
            std::cout << "MockTlsChannel: 读取成功，数据大小: " << DataSize << std::endl;
            
            auto Handler = std::move(ReadHandler);
            if (Handler) Handler(NetworkError::NONE, DataSize);
        }).detach();
    }

    void MockTlsChannel::AsyncWrite(const char* Data, size_t DataSize, TlsWriteHandler Handler)
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || IsShutdown)
        {
            if (Handler) Handler(NetworkError::NOT_INITIALIZED, 0);
            return;
        }

        if (HandshakeState != TlsHandshakeState::CONNECTED)
        {
            if (Handler) Handler(NetworkError::CONNECTION_FAILED, 0);
            return;
        }

        WriteHandler = std::move(Handler);

        std::cout << "MockTlsChannel: 开始异步写入，数据大小: " << DataSize << std::endl;
        std::cout << "MockTlsChannel: 数据内容: " << std::string(Data, DataSize) << std::endl;

        // 模拟异步写入过程
        std::thread([this, DataSize]() {
            // 模拟写入延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::lock_guard<std::mutex> Lock(StateMutex);
            
            if (IsShutdown)
            {
                return;
            }

            // 模拟写入成功
            std::cout << "MockTlsChannel: 写入成功，数据大小: " << DataSize << std::endl;
            
            auto Handler = std::move(WriteHandler);
            if (Handler) Handler(NetworkError::NONE, DataSize);
        }).detach();
    }

    bool MockTlsChannel::IsConnected() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        return IsInitialized && !IsShutdown && HandshakeState == TlsHandshakeState::CONNECTED;
    }

    bool MockTlsChannel::IsClosed() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        return IsShutdown || HandshakeState == TlsHandshakeState::CLOSED;
    }

    std::string MockTlsChannel::GetPeerCertificateSubject() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || HandshakeState != TlsHandshakeState::CONNECTED)
        {
            return "";
        }

        return "CN=MockPeer, O=MockOrganization, C=US";
    }

    std::string MockTlsChannel::GetLocalCertificateSubject() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized)
        {
            return "";
        }

        return "CN=MockServer, O=MockOrganization, C=US";
    }

    std::string MockTlsChannel::GetCipherSuite() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || HandshakeState != TlsHandshakeState::CONNECTED)
        {
            return "";
        }

        return "TLS_AES_256_GCM_SHA384";
    }

    std::string MockTlsChannel::GetProtocolVersion() const
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        
        if (!IsInitialized || HandshakeState != TlsHandshakeState::CONNECTED)
        {
            return "";
        }

        return "TLSv1.3";
    }

    void MockTlsChannel::SetSocketFd(uintptr_t Fd)
    {
        std::lock_guard<std::mutex> Lock(StateMutex);
        SocketFd = Fd;
        std::cout << "MockTlsChannel: 设置Socket文件描述符: " << Fd << std::endl;
    }

    void MockTlsChannel::ClearCallbacks()
    {
        HandshakeHandler = nullptr;
        ReadHandler = nullptr;
        WriteHandler = nullptr;
    }
}
