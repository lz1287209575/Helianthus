#include "Message/MessageQueue.h"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace Helianthus::Message
{
    MessageQueue::MessageQueue()
    {
        // Initialize with default configuration
        Config = MessageQueueConfig{};
        Stats = MessageStats{};
    }

    MessageQueue::~MessageQueue()
    {
        Shutdown();
    }

    MessageQueue::MessageQueue(MessageQueue&& Other) noexcept
        : Queue(std::move(Other.Queue))
        , Config(std::move(Other.Config))
        , InitializedFlag(Other.InitializedFlag.load())
        , IsThreadSafeEnabled(Other.IsThreadSafeEnabled.load())
        , ShuttingDownFlag(Other.ShuttingDownFlag.load())
        , Stats(std::move(Other.Stats))
        , DroppedMessageCount(Other.DroppedMessageCount.load())
        , MessageCallbackFunc(std::move(Other.MessageCallbackFunc))
        , QueueFullCallback(std::move(Other.QueueFullCallback))
        , QueueEmptyCallback(std::move(Other.QueueEmptyCallback))
        , AutoDequeueEnabled(Other.AutoDequeueEnabled.load())
        , AutoDequeueIntervalMs(Other.AutoDequeueIntervalMs.load())
        , StopAutoDequeue(Other.StopAutoDequeue.load())
        , TypeIndex(std::move(Other.TypeIndex))
    {
        // Reset other's state
        Other.InitializedFlag = false;
        Other.ShuttingDownFlag = false;
    }

    MessageQueue& MessageQueue::operator=(MessageQueue&& Other) noexcept
    {
        if (this != &Other)
        {
            Shutdown(); // Clean up current state
            
            Queue = std::move(Other.Queue);
            Config = std::move(Other.Config);
            InitializedFlag = Other.InitializedFlag.load();
            IsThreadSafeEnabled = Other.IsThreadSafeEnabled.load();
            ShuttingDownFlag = Other.ShuttingDownFlag.load();
            Stats = std::move(Other.Stats);
            DroppedMessageCount = Other.DroppedMessageCount.load();
            MessageCallbackFunc = std::move(Other.MessageCallbackFunc);
            QueueFullCallback = std::move(Other.QueueFullCallback);
            QueueEmptyCallback = std::move(Other.QueueEmptyCallback);
            AutoDequeueEnabled = Other.AutoDequeueEnabled.load();
            AutoDequeueIntervalMs = Other.AutoDequeueIntervalMs.load();
            StopAutoDequeue = Other.StopAutoDequeue.load();
            TypeIndex = std::move(Other.TypeIndex);
            
            // Reset other's state
            Other.InitializedFlag = false;
            Other.ShuttingDownFlag = false;
        }
        return *this;
    }

    MessageResult MessageQueue::Initialize(const MessageQueueConfig& Config)
    {
        if (InitializedFlag)
        {
            return MessageResult::ALREADY_EXISTS;
        }
        
        this->Config = Config;
        InitializedFlag = true;
        ShuttingDownFlag = false;
        
        return MessageResult::SUCCESS;
    }

    void MessageQueue::Shutdown()
    {
        if (!InitializedFlag)
        {
            return;
        }
        
        ShuttingDownFlag = true;
        
        // Stop auto dequeue if enabled
        if (AutoDequeueEnabled)
        {
            StopAutoDequeue = true;
            if (AutoDequeueThread.joinable())
            {
                AutoDequeueThread.join();
            }
        }
        
        // Wake up any waiting threads
        QueueCondition.notify_all();
        
        // Clear the queue
        Clear();
        
        InitializedFlag = false;
    }

    bool MessageQueue::IsInitialized() const
    {
        return InitializedFlag;
    }

    MessageResult MessageQueue::Enqueue(MessagePtr Message)
    {
        if (!Message)
        {
            return MessageResult::INVALID_MESSAGE;
        }
        
        return EnqueueInternal(Message);
    }

    MessageResult MessageQueue::EnqueueWithPriority(MessagePtr Message, MessagePriority Priority)
    {
        if (!Message)
        {
            return MessageResult::INVALID_MESSAGE;
        }
        
        Message->SetPriority(Priority);
        return EnqueueInternal(Message);
    }

    MessagePtr MessageQueue::Dequeue()
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
            return DequeueInternal();
        }
        else
        {
            return DequeueInternal();
        }
    }

    MessagePtr MessageQueue::DequeueWithTimeout(uint32_t TimeoutMs)
    {
        if (!IsThreadSafeEnabled)
        {
            return DequeueInternal();
        }
        
        std::unique_lock<std::mutex> Lock(QueueMutex);
        
        if (Queue.empty())
        {
            auto WaitResult = QueueCondition.wait_for(
                Lock,
                std::chrono::milliseconds(TimeoutMs),
                [this] { return !Queue.empty() || ShuttingDownFlag; }
            );
            
            if (!WaitResult || ShuttingDownFlag)
            {
                return nullptr;
            }
        }
        
        return DequeueInternal();
    }

    MessagePtr MessageQueue::Peek() const
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
            return Queue.empty() ? nullptr : Queue.top();
        }
        else
        {
            return Queue.empty() ? nullptr : Queue.top();
        }
    }

    MessagePtr MessageQueue::DequeueByPriority(MessagePriority MinPriority)
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        if (Queue.empty())
        {
            return nullptr;
        }
        
        auto TopMessage = Queue.top();
        if (static_cast<int>(TopMessage->GetPriority()) >= static_cast<int>(MinPriority))
        {
            return DequeueInternal();
        }
        
        return nullptr;
    }

    std::vector<MessagePtr> MessageQueue::DequeueByType(MessageType MsgType, uint32_t MaxCount)
    {
        std::vector<MessagePtr> Result;
        std::vector<MessagePtr> TempMessages;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        // Extract messages of the specified type
        while (!Queue.empty() && Result.size() < MaxCount)
        {
            auto Message = Queue.top();
            Queue.pop();
            
            if (Message->GetMessageType() == MsgType)
            {
                Result.push_back(Message);
                UpdateStatsOnDequeue(Message);
            }
            else
            {
                TempMessages.push_back(Message);
            }
        }
        
        // Put back non-matching messages
        for (const auto& Message : TempMessages)
        {
            Queue.push(Message);
        }
        
        return Result;
    }

    std::vector<MessagePtr> MessageQueue::DequeueBatch(uint32_t MaxCount)
    {
        std::vector<MessagePtr> Result;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        while (!Queue.empty() && Result.size() < MaxCount)
        {
            Result.push_back(DequeueInternal());
        }
        
        return Result;
    }

    size_t MessageQueue::GetSize() const
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        return Queue.size();
    }

    size_t MessageQueue::GetSizeByPriority(MessagePriority Priority) const
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        size_t Count = 0;
        auto TempQueue = Queue;
        
        while (!TempQueue.empty())
        {
            if (TempQueue.top()->GetPriority() == Priority)
            {
                Count++;
            }
            TempQueue.pop();
        }
        
        return Count;
    }

    bool MessageQueue::IsEmpty() const
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        return Queue.empty();
    }

    bool MessageQueue::IsFull() const
    {
        return GetSize() >= Config.MaxQueueSize;
    }

    uint32_t MessageQueue::GetMaxSize() const
    {
        return Config.MaxQueueSize;
    }

    void MessageQueue::SetMaxSize(uint32_t MaxSize)
    {
        Config.MaxQueueSize = MaxSize;
    }

    std::vector<MessagePtr> MessageQueue::FindMessages(
        std::function<bool(const MessagePtr&)> Predicate,
        uint32_t MaxCount
    ) const
    {
        std::vector<MessagePtr> Result;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        auto TempQueue = Queue;
        while (!TempQueue.empty() && Result.size() < MaxCount)
        {
            auto Message = TempQueue.top();
            TempQueue.pop();
            
            if (Predicate(Message))
            {
                Result.push_back(Message);
            }
        }
        
        return Result;
    }

    MessagePtr MessageQueue::FindFirstMessage(MessageType MsgType) const
    {
        auto Messages = FindMessages(
            [MsgType](const MessagePtr& Message) {
                return Message->GetMessageType() == MsgType;
            },
            1
        );
        
        return Messages.empty() ? nullptr : Messages[0];
    }

    size_t MessageQueue::CountMessagesByType(MessageType MsgType) const
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        size_t Count = 0;
        auto TempQueue = Queue;
        
        while (!TempQueue.empty())
        {
            if (TempQueue.top()->GetMessageType() == MsgType)
            {
                Count++;
            }
            TempQueue.pop();
        }
        
        return Count;
    }

    void MessageQueue::Clear()
    {
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        while (!Queue.empty())
        {
            Queue.pop();
        }
        
        std::lock_guard<std::mutex> TypeLock(TypeIndexMutex);
        TypeIndex.clear();
    }

    void MessageQueue::ClearByPriority(MessagePriority Priority)
    {
        std::vector<MessagePtr> TempMessages;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        // Remove messages with specified priority
        while (!Queue.empty())
        {
            auto Message = Queue.top();
            Queue.pop();
            
            if (Message->GetPriority() != Priority)
            {
                TempMessages.push_back(Message);
            }
        }
        
        // Put back non-matching messages
        for (const auto& Message : TempMessages)
        {
            Queue.push(Message);
        }
    }

    void MessageQueue::ClearByType(MessageType MsgType)
    {
        std::vector<MessagePtr> TempMessages;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        // Remove messages with specified type
        while (!Queue.empty())
        {
            auto Message = Queue.top();
            Queue.pop();
            
            if (Message->GetMessageType() != MsgType)
            {
                TempMessages.push_back(Message);
            }
        }
        
        // Put back non-matching messages
        for (const auto& Message : TempMessages)
        {
            Queue.push(Message);
        }
    }

    MessageResult MessageQueue::RemoveMessage(MessageId MsgId)
    {
        std::vector<MessagePtr> TempMessages;
        bool Found = false;
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        // Find and remove the message
        while (!Queue.empty())
        {
            auto Message = Queue.top();
            Queue.pop();
            
            if (Message->GetMessageId() == MsgId)
            {
                Found = true;
                // Don't add back to temp messages
            }
            else
            {
                TempMessages.push_back(Message);
            }
        }
        
        // Put back remaining messages
        for (const auto& Message : TempMessages)
        {
            Queue.push(Message);
        }
        
        return Found ? MessageResult::SUCCESS : MessageResult::NOT_FOUND;
    }

    MessageStats MessageQueue::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        MessageStats LocalStats = Stats;
        LocalStats.QueueSize = GetSize();
        return LocalStats;
    }

    void MessageQueue::ResetStats()
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Stats = MessageStats{};
        DroppedMessageCount = 0;
    }

    uint32_t MessageQueue::GetDroppedMessageCount() const
    {
        return DroppedMessageCount;
    }

    void MessageQueue::UpdateConfig(const MessageQueueConfig& Config)
    {
        this->Config = Config;
    }

    MessageQueueConfig MessageQueue::GetCurrentConfig() const
    {
        return Config;
    }

    void MessageQueue::SetMessageCallback(MessageCallback Callback)
    {
        MessageCallbackFunc = std::move(Callback);
    }

    void MessageQueue::SetQueueFullCallback(std::function<void(MessagePtr)> Callback)
    {
        QueueFullCallback = std::move(Callback);
    }

    void MessageQueue::SetQueueEmptyCallback(std::function<void()> Callback)
    {
        QueueEmptyCallback = std::move(Callback);
    }

    void MessageQueue::RemoveAllCallbacks()
    {
        MessageCallbackFunc = nullptr;
        QueueFullCallback = nullptr;
        QueueEmptyCallback = nullptr;
    }

    MessageResult MessageQueue::SaveToFile(const std::string& FilePath) const
    {
        // Placeholder implementation
        // In real implementation, would serialize all messages to file
        return MessageResult::SUCCESS;
    }

    MessageResult MessageQueue::LoadFromFile(const std::string& FilePath)
    {
        // Placeholder implementation
        // In real implementation, would deserialize messages from file
        return MessageResult::SUCCESS;
    }

    void MessageQueue::EnableThreadSafety(bool Enable)
    {
        IsThreadSafeEnabled = Enable;
    }

    bool MessageQueue::IsThreadSafe() const
    {
        return IsThreadSafeEnabled;
    }

    MessageResult MessageQueue::WaitForMessage(MessageType MsgType, uint32_t TimeoutMs)
    {
        if (!IsThreadSafeEnabled)
        {
            return MessageResult::FAILED;
        }
        
        std::unique_lock<std::mutex> Lock(QueueMutex);
        
        auto WaitResult = QueueCondition.wait_for(
            Lock,
            std::chrono::milliseconds(TimeoutMs),
            [this, MsgType] {
                return FindFirstMessage(MsgType) != nullptr || ShuttingDownFlag;
            }
        );
        
        if (!WaitResult || ShuttingDownFlag)
        {
            return MessageResult::TIMEOUT;
        }
        
        return MessageResult::SUCCESS;
    }

    void MessageQueue::EnableAutoDequeue(bool Enable, uint32_t IntervalMs)
    {
        if (Enable && !AutoDequeueEnabled)
        {
            AutoDequeueEnabled = true;
            AutoDequeueIntervalMs = IntervalMs;
            StopAutoDequeue = false;
            
            AutoDequeueThread = std::thread(&MessageQueue::AutoDequeueLoop, this);
        }
        else if (!Enable && AutoDequeueEnabled)
        {
            AutoDequeueEnabled = false;
            StopAutoDequeue = true;
            
            if (AutoDequeueThread.joinable())
            {
                AutoDequeueThread.join();
            }
        }
    }

    bool MessageQueue::IsAutoDequeueEnabled() const
    {
        return AutoDequeueEnabled;
    }

    // Private helper methods
    MessagePtr MessageQueue::DequeueInternal()
    {
        if (Queue.empty())
        {
            return nullptr;
        }
        
        auto Message = Queue.top();
        Queue.pop();
        
        UpdateStatsOnDequeue(Message);
        TriggerCallbacks(Message);
        
        return Message;
    }

    MessageResult MessageQueue::EnqueueInternal(MessagePtr Message)
    {
        if (!InitializedFlag || ShuttingDownFlag)
        {
            return MessageResult::FAILED;
        }
        
        if (IsThreadSafeEnabled)
        {
            std::lock_guard<std::mutex> Lock(QueueMutex);
        }
        
        // Check if queue is full
        if (Queue.size() >= Config.MaxQueueSize)
        {
            DroppedMessageCount++;
            
            if (QueueFullCallback)
            {
                QueueFullCallback(Message);
            }
            
            return MessageResult::QUEUE_FULL;
        }
        
        Queue.push(Message);
        UpdateStatsOnEnqueue(Message);
        
        // Notify waiting threads
        if (IsThreadSafeEnabled)
        {
            QueueCondition.notify_one();
        }
        
        return MessageResult::SUCCESS;
    }

    void MessageQueue::UpdateStatsOnEnqueue(const MessagePtr& Message)
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Stats.MessagesSent++;
        Stats.BytesSent += Message->GetTotalSize();
        Stats.QueueSize = Queue.size();
        if (Stats.QueueSize > Stats.MaxQueueSize)
        {
            Stats.MaxQueueSize = Stats.QueueSize;
        }
    }

    void MessageQueue::UpdateStatsOnDequeue(const MessagePtr& Message)
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Stats.MessagesReceived++;
        Stats.BytesReceived += Message->GetTotalSize();
        Stats.QueueSize = Queue.size();
    }

    void MessageQueue::TriggerCallbacks(const MessagePtr& Message)
    {
        if (MessageCallbackFunc)
        {
            MessageCallbackFunc(Message);
        }
        
        if (Queue.empty() && QueueEmptyCallback)
        {
            QueueEmptyCallback();
        }
    }

    void MessageQueue::AutoDequeueLoop()
    {
        while (!StopAutoDequeue && AutoDequeueEnabled)
        {
            auto Message = Dequeue();
            if (Message && MessageCallbackFunc)
            {
                MessageCallbackFunc(Message);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(AutoDequeueIntervalMs));
        }
    }

} // namespace Helianthus::Message