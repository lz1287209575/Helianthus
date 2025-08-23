#pragma once

#include "Shared/Network/NetworkTypes.h"
#include <functional>
#include <memory>
#include <vector>

namespace Helianthus::Network::Security
{
    // TLS握手状态
    enum class TlsHandshakeState
    {
        INITIAL,        // 初始状态
        CONNECTING,     // 正在连接
        CONNECTED,      // 连接成功
        FAILED,         // 连接失败
        CLOSED          // 已关闭
    };

    // TLS配置选项
    struct TlsConfig
    {
        std::string CertificateFile;      // 证书文件路径
        std::string PrivateKeyFile;       // 私钥文件路径
        std::string CaCertificateFile;    // CA证书文件路径
        std::vector<std::string> CipherSuites; // 支持的加密套件
        bool VerifyPeer = true;           // 是否验证对端证书
        bool RequireClientCertificate = false; // 是否要求客户端证书
        uint32_t HandshakeTimeoutMs = 30000;   // 握手超时时间
    };

    // TLS异步操作回调类型
    using TlsHandshakeHandler = std::function<void(NetworkError, TlsHandshakeState)>;
    using TlsReadHandler = std::function<void(NetworkError, size_t)>;
    using TlsWriteHandler = std::function<void(NetworkError, size_t)>;

    // TLS通道抽象接口
    class ITlsChannel
    {
    public:
        virtual ~ITlsChannel() = default;

        // 基础操作
        virtual NetworkError Initialize(const TlsConfig& Config) = 0;
        virtual void Shutdown() = 0;
        
        // 握手操作
        virtual void AsyncHandshake(TlsHandshakeHandler Handler) = 0;
        virtual TlsHandshakeState GetHandshakeState() const = 0;
        
        // 数据传输
        virtual void AsyncRead(char* Buffer, size_t BufferSize, TlsReadHandler Handler) = 0;
        virtual void AsyncWrite(const char* Data, size_t DataSize, TlsWriteHandler Handler) = 0;
        
        // 状态查询
        virtual bool IsConnected() const = 0;
        virtual bool IsClosed() const = 0;
        
        // 获取证书信息
        virtual std::string GetPeerCertificateSubject() const = 0;
        virtual std::string GetLocalCertificateSubject() const = 0;
        
        // 获取加密信息
        virtual std::string GetCipherSuite() const = 0;
        virtual std::string GetProtocolVersion() const = 0;

        // 设置底层Socket文件描述符
        virtual void SetSocketFd(uintptr_t SocketFd) = 0;
    };

    // 创建TLS通道的工厂函数
    std::unique_ptr<ITlsChannel> CreateTlsChannel();
}
