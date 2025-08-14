#pragma once

#include "Shared/Common/Types.h"
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace Helianthus::Message
{
    // Message priority levels
    enum class MessagePriority : uint8_t
    {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };

    // Message types for routing
    enum class MessageType : uint16_t
    {
        // System messages
        SYSTEM_HEARTBEAT = 1000,
        SYSTEM_SHUTDOWN = 1001,
        SYSTEM_STATUS = 1002,
        
        // Network messages
        NETWORK_CONNECTION_ESTABLISHED = 2000,
        NETWORK_CONNECTION_LOST = 2001,
        NETWORK_DATA_RECEIVED = 2002,
        
        // Service messages
        SERVICE_REGISTER = 3000,
        SERVICE_UNREGISTER = 3001,
        SERVICE_DISCOVERY = 3002,
        
        // Game messages  
        GAME_PLAYER_JOIN = 4000,
        GAME_PLAYER_LEAVE = 4001,
        GAME_STATE_UPDATE = 4002,
        
        // Authentication messages
        AUTH_LOGIN_REQUEST = 5000,
        AUTH_LOGIN_RESPONSE = 5001,
        AUTH_LOGOUT = 5002,
        
        // Custom message range starts at 10000
        CUSTOM_MESSAGE_START = 10000
    };

    // Message delivery modes
    enum class DeliveryMode : uint8_t
    {
        FIRE_AND_FORGET = 0,    // No acknowledgment required
        RELIABLE = 1,           // Requires acknowledgment
        ORDERED = 2,            // Must be delivered in order
        BROADCAST = 3,          // Send to all subscribers
        MULTICAST = 4           // Send to specific group
    };

    // Message result codes
    enum class MessageResult : int32_t
    {
        SUCCESS = 0,
        FAILED = -1,
        TIMEOUT = -2,
        QUEUE_FULL = -3,
        INVALID_MESSAGE = -4,
        NO_SUBSCRIBERS = -5,
        SERIALIZATION_FAILED = -6,
        DESERIALIZATION_FAILED = -7,
        ROUTING_FAILED = -8,
        ALREADY_EXISTS = -9,
        NOT_FOUND = -10
    };

    // Message ID and related types
    using MessageId = uint64_t;
    using SubscriberId = uint64_t;
    using TopicId = uint32_t;
    
    static constexpr MessageId InvalidMessageId = 0;
    static constexpr SubscriberId InvalidSubscriberId = 0;
    static constexpr TopicId InvalidTopicId = 0;

    // Message header structure
    struct MessageHeader
    {
        MessageId MsgId = InvalidMessageId;
        MessageType MsgType = MessageType::CUSTOM_MESSAGE_START;
        MessagePriority Priority = MessagePriority::NORMAL;
        DeliveryMode PostMode = DeliveryMode::FIRE_AND_FORGET;
        Common::ServerId SenderId = Common::InvalidServerId;
        Common::ServerId ReceiverId = Common::InvalidServerId;
        TopicId ThemId = InvalidTopicId;
        Common::TimestampMs Timestamp = 0;
        uint32_t PayloadSize = 0;
        uint32_t Checksum = 0;
        uint32_t SequenceNumber = 0;
        uint32_t RetryCount = 0;
        uint32_t MaxRetries = 3;
        uint32_t TimeoutMs = 5000;
    };

    // Message statistics
    struct MessageStats
    {
        uint64_t MessagesSent = 0;
        uint64_t MessagesReceived = 0;
        uint64_t MessagesDropped = 0;
        uint64_t BytesSent = 0;
        uint64_t BytesReceived = 0;
        uint64_t AverageLatencyMs = 0;
        uint64_t MaxLatencyMs = 0;
        uint32_t QueueSize = 0;
        uint32_t MaxQueueSize = 0;
    };

    // Message queue configuration
    struct MessageQueueConfig
    {
        uint32_t MaxQueueSize = 10000;
        uint32_t MaxMessageSize = 1024 * 1024; // 1MB
        uint32_t DefaultTimeoutMs = 5000;
        uint32_t MaxRetries = 3;
        bool EnablePersistence = false;
        bool EnableCompression = false;
        bool EnableEncryption = false;
        std::string PersistencePath = "data/messages/";
    };

    // Forward declarations
    class Message;
    class IMessageQueue;
    class IMessageBus;
    class MessageRouter;

    // Smart pointer type definitions
    using MessagePtr = std::shared_ptr<Message>;
    using MessageQueuePtr = std::shared_ptr<IMessageQueue>;
    using MessageBusPtr = std::shared_ptr<IMessageBus>;
    using MessageRouterPtr = std::shared_ptr<MessageRouter>;

    // Callback function types
    using MessageCallback = std::function<void(MessagePtr Message)>;
    using MessageResultCallback = std::function<void(MessageId MsgId, MessageResult Result)>;
    using TopicCallback = std::function<void(TopicId ThemId, MessagePtr Message)>;

} // namespace Helianthus::Message