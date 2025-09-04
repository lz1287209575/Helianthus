#include "Shared/MessageQueue/MessageQueue.h"

#include <iostream>

int main()
{
    // 初始化队列
    auto Queue = std::make_unique<Helianthus::MessageQueue::MessageQueue>();
    Queue->Initialize();

    // 创建队列
    Helianthus::MessageQueue::QueueConfig Config;
    Config.Name = "example_queue";
    Config.Persistence = Helianthus::MessageQueue::PersistenceMode::MEMORY_ONLY;
    Config.MaxSize = 1000;
    Queue->CreateQueue(Config);

    // 发送消息
    for (int i = 0; i < 5; ++i)
    {
        auto Message = std::make_shared<Helianthus::MessageQueue::Message>();
        Message->Header.Type = Helianthus::MessageQueue::MessageType::TEXT;
        std::string Payload = "Message " + std::to_string(i);
        Message->Payload.Data.assign(Payload.begin(), Payload.end());

        Queue->SendMessage(Config.Name, Message);
        std::cout << "Sent: " << Payload << std::endl;
    }

    // 接收消息
    std::cout << "\nReceiving messages:" << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        Helianthus::MessageQueue::MessagePtr ReceivedMessage;
        Queue->ReceiveMessage(Config.Name, ReceivedMessage);

        std::string Payload(ReceivedMessage->Payload.Data.begin(),
                            ReceivedMessage->Payload.Data.end());
        std::cout << "Received: " << Payload << std::endl;
    }

    return 0;
}
