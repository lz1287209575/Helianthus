#pragma once

#include <memory>
#include <string>
#include <vector>

#include "MessageTypes.h"

namespace Helianthus::Message
{
/**
 * @brief Core message class for inter-service communication
 *
 * This class encapsulates message data, headers, and provides
 * serialization/deserialization capabilities for network transmission.
 */
class Message
{
public:
    // Constructors
    Message();
    explicit Message(MessageType MsgType);
    Message(MessageType MsgType, const std::vector<char>& Payload);
    Message(MessageType MsgType, const std::string& JsonPayload);

    // Copy and move constructors
    Message(const Message& Other);
    Message(Message&& Other) noexcept;
    Message& operator=(const Message& Other);
    Message& operator=(Message&& Other) noexcept;

    virtual ~Message() = default;

    // Header access
    MessageHeader& GetHeader()
    {
        return Header;
    }
    const MessageHeader& GetHeader() const
    {
        return Header;
    }
    void SetHeader(const MessageHeader& Header)
    {
        this->Header = Header;
    }

    // Payload management
    void SetPayload(const std::vector<char>& Payload);
    void SetPayload(const std::string& JsonPayload);
    void SetPayload(const char* Data, size_t Size);

    const std::vector<char>& GetPayload() const
    {
        return Payload;
    }
    std::vector<char>& GetPayload()
    {
        return Payload;
    }

    size_t GetPayloadSize() const
    {
        return Payload.size();
    }
    bool HasPayload() const
    {
        return !Payload.empty();
    }

    // JSON payload helpers
    std::string GetJsonPayload() const;
    bool SetJsonPayload(const std::string& Json);

    // Message properties
    MessageId GetMessageId() const
    {
        return Header.MsgId;
    }
    void SetMessageId(MessageId Id)
    {
        Header.MsgId = Id;
    }

    MessageType GetMessageType() const
    {
        return Header.MsgType;
    }
    void SetMessageType(MessageType Type)
    {
        Header.MsgType = Type;
    }

    MessagePriority GetPriority() const
    {
        return Header.Priority;
    }
    void SetPriority(MessagePriority Priority)
    {
        Header.Priority = Priority;
    }

    DeliveryMode GetDeliveryMode() const
    {
        return Header.PostMode;
    }
    void SetDeliveryMode(DeliveryMode Mode)
    {
        Header.PostMode = Mode;
    }

    Common::ServerId GetSenderId() const
    {
        return Header.SenderId;
    }
    void SetSenderId(Common::ServerId Id)
    {
        Header.SenderId = Id;
    }

    Common::ServerId GetReceiverId() const
    {
        return Header.ReceiverId;
    }
    void SetReceiverId(Common::ServerId Id)
    {
        Header.ReceiverId = Id;
    }

    TopicId GetTopicId() const
    {
        return Header.ThemId;
    }
    void SetTopicId(TopicId Id)
    {
        Header.ThemId = Id;
    }

    Common::TimestampMs GetTimestamp() const
    {
        return Header.Timestamp;
    }
    void SetTimestamp(Common::TimestampMs Timestamp)
    {
        Header.Timestamp = Timestamp;
    }

    uint32_t GetSequenceNumber() const
    {
        return Header.SequenceNumber;
    }
    void SetSequenceNumber(uint32_t SequenceNumber)
    {
        Header.SequenceNumber = SequenceNumber;
    }

    // Serialization
    std::vector<char> Serialize() const;
    bool Deserialize(const std::vector<char>& Data);
    bool Deserialize(const char* Data, size_t Size);

    // Validation
    bool IsValid() const;
    uint32_t CalculateChecksum() const;
    bool ValidateChecksum() const;
    void UpdateChecksum();

    // Utility functions
    size_t GetTotalSize() const;  // Header + Payload size
    void Reset();                 // Clear all data
    Message Clone() const;        // Deep copy

    // String representation for debugging
    std::string ToString() const;
    std::string GetHeaderString() const;

    // Compression (if enabled)
    bool Compress();
    bool Decompress();
    bool IsCompressed() const
    {
        return IsCompressedFlag;
    }

    // Encryption (if enabled)
    bool Encrypt(const std::string& Key);
    bool Decrypt(const std::string& Key);
    bool IsEncrypted() const
    {
        return IsEncryptedFlag;
    }

    // Static factory methods
    static MessagePtr Create(MessageType MsgType);
    static MessagePtr Create(MessageType MsgType, const std::vector<char>& Payload);
    static MessagePtr Create(MessageType MsgType, const std::string& JsonPayload);
    static MessagePtr CreateResponse(const Message& OriginalMessage, MessageType ResponseType);

private:
    MessageHeader Header;
    std::vector<char> Payload;
    bool IsCompressedFlag = false;
    bool IsEncryptedFlag = false;

    void UpdateHeaderFromPayload();
    uint32_t CalculateCRC32(const char* Data, size_t Size) const;
};

}  // namespace Helianthus::Message