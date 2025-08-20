#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Helianthus::Network::Asio
{
// 长度前缀协议：4字节长度 + 消息内容
class MessageProtocol
{
public:
    using MessageHandler = std::function<void(const std::string& Message)>;

    MessageProtocol();

    // 处理接收到的数据，可能产生多个完整消息
    void ProcessReceivedData(const char* Data, size_t Size);

    // 设置消息处理回调
    void SetMessageHandler(MessageHandler Handler);

    // 将消息编码为带长度前缀的格式
    static std::vector<char> EncodeMessage(const std::string& Message);

    // 获取当前缓冲区大小（用于测试）
    size_t GetBufferSize() const
    {
        return ReceiveBuffer.size();
    }

    // 重置状态
    void Reset();

private:
    enum class State
    {
        WaitingForLength,  // 等待长度字段
        WaitingForMessage  // 等待消息内容
    };

    void ProcessBuffer();

    std::vector<char> ReceiveBuffer;
    State CurrentState = State::WaitingForLength;
    uint32_t ExpectedMessageLength = 0;
    MessageHandler MessageHandlerCallback;
};
}  // namespace Helianthus::Network::Asio
