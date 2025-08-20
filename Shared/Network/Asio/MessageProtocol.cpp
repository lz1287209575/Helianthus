#include "Shared/Network/Asio/MessageProtocol.h"

#include <algorithm>
#include <cstring>

namespace Helianthus::Network::Asio
{
MessageProtocol::MessageProtocol()
    : ReceiveBuffer(),
      CurrentState(State::WaitingForLength),
      ExpectedMessageLength(0),
      MessageHandlerCallback()
{
}

void MessageProtocol::ProcessReceivedData(const char* Data, size_t Size)
{
    if (Size == 0)
        return;

    // 将新数据追加到缓冲区
    ReceiveBuffer.insert(ReceiveBuffer.end(), Data, Data + Size);

    // 处理缓冲区中的数据
    ProcessBuffer();
}

void MessageProtocol::SetMessageHandler(MessageHandler Handler)
{
    MessageHandlerCallback = std::move(Handler);
}

std::vector<char> MessageProtocol::EncodeMessage(const std::string& Message)
{
    std::vector<char> Encoded;

    // 写入长度（小端序）
    uint32_t Length = static_cast<uint32_t>(Message.size());
    Encoded.resize(sizeof(uint32_t) + Message.size());

    std::memcpy(Encoded.data(), &Length, sizeof(uint32_t));
    std::memcpy(Encoded.data() + sizeof(uint32_t), Message.data(), Message.size());

    return Encoded;
}

void MessageProtocol::Reset()
{
    ReceiveBuffer.clear();
    CurrentState = State::WaitingForLength;
    ExpectedMessageLength = 0;
}

void MessageProtocol::ProcessBuffer()
{
    while (true)
    {
        if (CurrentState == State::WaitingForLength)
        {
            // 需要至少4字节来读取长度
            if (ReceiveBuffer.size() < sizeof(uint32_t))
            {
                break;
            }

            // 读取长度（小端序）
            std::memcpy(&ExpectedMessageLength, ReceiveBuffer.data(), sizeof(uint32_t));

            // 移除已读取的长度字段
            ReceiveBuffer.erase(ReceiveBuffer.begin(), ReceiveBuffer.begin() + sizeof(uint32_t));

            CurrentState = State::WaitingForMessage;
        }
        else if (CurrentState == State::WaitingForMessage)
        {
            // 检查是否有足够的数据读取完整消息
            if (ReceiveBuffer.size() < ExpectedMessageLength)
            {
                break;
            }

            // 提取消息
            std::string Message(ReceiveBuffer.begin(),
                                ReceiveBuffer.begin() + ExpectedMessageLength);

            // 移除已读取的消息内容
            ReceiveBuffer.erase(ReceiveBuffer.begin(),
                                ReceiveBuffer.begin() + ExpectedMessageLength);

            // 回调处理消息
            if (MessageHandlerCallback)
            {
                MessageHandlerCallback(Message);
            }

            // 重置状态，准备读取下一个消息
            CurrentState = State::WaitingForLength;
            ExpectedMessageLength = 0;
        }
    }
}
}  // namespace Helianthus::Network::Asio
