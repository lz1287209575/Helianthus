#pragma once

#include "Message/MessageTypes.h"
#include "Message/Message.h"
#include <functional>
#include <vector>

namespace Helianthus::Message
{
    /**
     * @brief Abstract interface for message queue
     * 
     * Provides message queuing functionality with priority handling,
     * persistence options, and callback-based message processing.
     */
    class IMessageQueue
    {
    public:
        virtual ~IMessageQueue() = default;

        // Initialization and lifecycle
        virtual MessageResult Initialize(const MessageQueueConfig& Config) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsInitialized() const = 0;

        // Message operations
        virtual MessageResult Enqueue(MessagePtr Message) = 0;
        virtual MessageResult EnqueueWithPriority(MessagePtr Message, MessagePriority Priority) = 0;
        virtual MessagePtr Dequeue() = 0;
        virtual MessagePtr DequeueWithTimeout(uint32_t TimeoutMs) = 0;
        virtual MessagePtr Peek() const = 0;

        // Priority-based operations
        virtual MessagePtr DequeueByPriority(MessagePriority MinPriority) = 0;
        virtual std::vector<MessagePtr> DequeueByType(MessageType MessageType, uint32_t MaxCount = 10) = 0;
        virtual std::vector<MessagePtr> DequeueBatch(uint32_t MaxCount = 10) = 0;

        // Queue information
        virtual size_t GetSize() const = 0;
        virtual size_t GetSizeByPriority(MessagePriority Priority) const = 0;
        virtual bool IsEmpty() const = 0;
        virtual bool IsFull() const = 0;
        virtual uint32_t GetMaxSize() const = 0;
        virtual void SetMaxSize(uint32_t MaxSize) = 0;

        // Message filtering and search
        virtual std::vector<MessagePtr> FindMessages(
            std::function<bool(const MessagePtr&)> Predicate,
            uint32_t MaxCount = 10
        ) const = 0;
        virtual MessagePtr FindFirstMessage(MessageType MessageType) const = 0;
        virtual size_t CountMessagesByType(MessageType MessageType) const = 0;

        // Queue management
        virtual void Clear() = 0;
        virtual void ClearByPriority(MessagePriority Priority) = 0;
        virtual void ClearByType(MessageType MessageType) = 0;
        virtual MessageResult RemoveMessage(MessageId MessageId) = 0;

        // Statistics and monitoring
        virtual MessageStats GetStats() const = 0;
        virtual void ResetStats() = 0;
        virtual uint32_t GetDroppedMessageCount() const = 0;

        // Configuration
        virtual void UpdateConfig(const MessageQueueConfig& Config) = 0;
        virtual MessageQueueConfig GetCurrentConfig() const = 0;

        // Callback registration
        virtual void SetMessageCallback(MessageCallback Callback) = 0;
        virtual void SetQueueFullCallback(std::function<void(MessagePtr DroppedMessage)> Callback) = 0;
        virtual void SetQueueEmptyCallback(std::function<void()> Callback) = 0;
        virtual void RemoveAllCallbacks() = 0;

        // Persistence (if enabled)
        virtual MessageResult SaveToFile(const std::string& FilePath) const = 0;
        virtual MessageResult LoadFromFile(const std::string& FilePath) = 0;

        // Thread-safe operations
        virtual void EnableThreadSafety(bool Enable = true) = 0;
        virtual bool IsThreadSafe() const = 0;

        // Advanced operations
        virtual MessageResult WaitForMessage(MessageType MessageType, uint32_t TimeoutMs) = 0;
        virtual void EnableAutoDequeue(bool Enable, uint32_t IntervalMs = 100) = 0;
        virtual bool IsAutoDequeueEnabled() const = 0;
    };

} // namespace Helianthus::Message