#pragma once

#include "Shared/Network/Security/ITlsChannel.h"

#include <memory>
#include <mutex>

namespace Helianthus::Network::Security
{
// 模拟TLS通道实现（用于演示架构）
class MockTlsChannel : public ITlsChannel
{
public:
    MockTlsChannel();
    virtual ~MockTlsChannel();

    // 禁用拷贝
    MockTlsChannel(const MockTlsChannel&) = delete;
    MockTlsChannel& operator=(const MockTlsChannel&) = delete;

    // ITlsChannel接口实现
    NetworkError Initialize(const TlsConfig& Config) override;
    void Shutdown() override;

    void AsyncHandshake(TlsHandshakeHandler Handler) override;
    TlsHandshakeState GetHandshakeState() const override;

    void AsyncRead(char* Buffer, size_t BufferSize, TlsReadHandler Handler) override;
    void AsyncWrite(const char* Data, size_t DataSize, TlsWriteHandler Handler) override;

    bool IsConnected() const override;
    bool IsClosed() const override;

    std::string GetPeerCertificateSubject() const override;
    std::string GetLocalCertificateSubject() const override;

    std::string GetCipherSuite() const override;
    std::string GetProtocolVersion() const override;

    // 设置底层Socket文件描述符
    void SetSocketFd(uintptr_t SocketFd);

private:
    // 配置
    TlsConfig Config;
    TlsHandshakeState HandshakeState;

    // 底层Socket
    uintptr_t SocketFd;

    // 回调
    TlsHandshakeHandler HandshakeHandler;
    TlsReadHandler ReadHandler;
    TlsWriteHandler WriteHandler;

    // 状态
    bool IsInitialized;
    bool IsShutdown;

    // 线程安全
    mutable std::mutex StateMutex;

    // 内部方法
    void ClearCallbacks();
};
}  // namespace Helianthus::Network::Security
