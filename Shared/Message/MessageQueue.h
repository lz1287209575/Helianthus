#pragma once

#include "Message/IMessageQueue.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace Helianthus::Message
{
    /**
     * @brief Basic message queue implementation with priority support
     * 
     * Thread-safe message queue that supports priority-based ordering,
     * message filtering, statistics, and optional persistence.
     */
    class MessageQueue : public IMessageQueue
    {
    public:
        MessageQueue();
        virtual ~MessageQueue();

        // Disable copy, allow move
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;
        MessageQueue(MessageQueue&& Other) noexcept;
        MessageQueue& operator=(MessageQueue&& Other) noexcept;

        // IMessageQueue implementation
        MessageResult Initialize(const MessageQueueConfig& Config) override;
        void Shutdown() override;
        bool IsInitialized() const override;

        MessageResult Enqueue(MessagePtr Message) override;
        MessageResult EnqueueWithPriority(MessagePtr Message, MessagePriority Priority) override;
        MessagePtr Dequeue() override;
        MessagePtr DequeueWithTimeout(uint32_t TimeoutMs) override;
        MessagePtr Peek() const override;

        MessagePtr DequeueByPriority(MessagePriority MinPriority) override;
        std::vector<MessagePtr> DequeueByType(MessageType MsgType, uint32_t MaxCount = 10) override;
        std::vector<MessagePtr> DequeueBatch(uint32_t MaxCount = 10) override;

        size_t GetSize() const override;
        size_t GetSizeByPriority(MessagePriority Priority) const override;
        bool IsEmpty() const override;
        bool IsFull() const override;
        uint32_t GetMaxSize() const override;
        void SetMaxSize(uint32_t MaxSize) override;

        std::vector<MessagePtr> FindMessages(
            std::function<bool(const MessagePtr&)> Predicate,
            uint32_t MaxCount = 10
        ) const override;
        MessagePtr FindFirstMessage(MessageType MsgType) const override;
        size_t CountMessagesByType(MessageType MsgType) const override;

        void Clear() override;
        void ClearByPriority(MessagePriority Priority) override;
        void ClearByType(MessageType MsgType) override;
        MessageResult RemoveMessage(MessageId MsgId) override;

        MessageStats GetStats() const override;
        void ResetStats() override;
        uint32_t GetDroppedMessageCount() const override;

        void UpdateConfig(const MessageQueueConfig& Config) override;
        MessageQueueConfig GetCurrentConfig() const override;

        void SetMessageCallback(MessageCallback Callback) override;
        void SetQueueFullCallback(std::function<void(MessagePtr)> Callback) override;
        void SetQueueEmptyCallback(std::function<void()> Callback) override;
        void RemoveAllCallbacks() override;

        MessageResult SaveToFile(const std::string& FilePath) const override;
        MessageResult LoadFromFile(const std::string& FilePath) override;

        void EnableThreadSafety(bool Enable = true) override;
        bool IsThreadSafe() const override;

        MessageResult WaitForMessage(MessageType MsgType, uint32_t TimeoutMs) override;
        void EnableAutoDequeue(bool Enable, uint32_t IntervalMs = 100) override;
        bool IsAutoDequeueEnabled() const override;

    private:
        // Priority queue comparison
        struct MessagePriorityComparator
        {
            bool operator()(const MessagePtr& Lhs, const MessagePtr& Rhs) const
            {
                // Higher priority values have higher priority
                auto LhsPriority = static_cast<int>(Lhs->GetPriority());
                auto RhsPriority = static_cast<int>(Rhs->GetPriority());
                
                if (LhsPriority != RhsPriority)
                {
                    return LhsPriority < RhsPriority; // Lower values = higher priority
                }
                
                // If same priority, older messages have higher priority
                return Lhs->GetTimestamp() > Rhs->GetTimestamp();
            }
        };

        using PriorityQueue = std::priority_queue<MessagePtr, std::vector<MessagePtr>, MessagePriorityComparator>;

        // Internal helper methods
        MessagePtr DequeueInternal();
        MessageResult EnqueueInternal(MessagePtr Message);
        void UpdateStatsOnEnqueue(const MessagePtr& Message);
        void UpdateStatsOnDequeue(const MessagePtr& Message);
        void TriggerCallbacks(const MessagePtr& Message);
        void AutoDequeueLoop();

    private:
        // Core queue and synchronization
        PriorityQueue Queue;
        mutable std::mutex QueueMutex;
        std::condition_variable QueueCondition;
        
        // Configuration and state
        MessageQueueConfig Config;
        std::atomic<bool> InitializedFlag = false;
        std::atomic<bool> IsThreadSafeEnabled = true;
        std::atomic<bool> ShuttingDownFlag = false;
        
        // Statistics
        mutable std::mutex StatsMutex;
        MessageStats Stats;
        std::atomic<uint32_t> DroppedMessageCount = 0;
        
        // Callbacks
        MessageCallback MessageCallbackFunc;
        std::function<void(MessagePtr)> QueueFullCallback;
        std::function<void()> QueueEmptyCallback;
        
        // Auto dequeue
        std::atomic<bool> AutoDequeueEnabled = false;
        std::atomic<uint32_t> AutoDequeueIntervalMs = 100;
        std::thread AutoDequeueThread;
        std::atomic<bool> StopAutoDequeue = false;
        
        // Message type indexing for fast lookup
        mutable std::mutex TypeIndexMutex;
        std::unordered_map<MessageType, std::vector<MessagePtr>> TypeIndex;
    };

} // namespace Helianthus::Message