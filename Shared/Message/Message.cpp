#include "Message/Message.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace Helianthus::Message
{
namespace
{
// Message header magic number for validation
constexpr uint32_t MESSAGE_HEADER_MAGIC = 0x48454C49;  // "HELI" in hex

// Generate unique message ID
std::atomic<MessageId> NextMessageId = 1;

MessageId GenerateMessageId()
{
    return NextMessageId.fetch_add(1, std::memory_order_relaxed);
}

// Simple CRC32 implementation
uint32_t CRC32Table[256];
bool CRC32TableInitialized = false;

void InitializeCRC32Table()
{
    if (CRC32TableInitialized)
        return;

    for (uint32_t I = 0; I < 256; I++)
    {
        uint32_t CRC = I;
        for (uint32_t J = 0; J < 8; J++)
        {
            if (CRC & 1)
                CRC = (CRC >> 1) ^ 0xEDB88320;
            else
                CRC >>= 1;
        }
        CRC32Table[I] = CRC;
    }
    CRC32TableInitialized = true;
}

uint32_t CalculateCRC32(const char* Data, size_t Length)
{
    InitializeCRC32Table();
    uint32_t CRC = 0xFFFFFFFF;
    for (size_t I = 0; I < Length; I++)
    {
        uint8_t Byte = static_cast<uint8_t>(Data[I]);
        uint8_t Index = static_cast<uint8_t>((CRC & 0xFF) ^ Byte);
        CRC = (CRC >> 8) ^ CRC32Table[Index];
    }
    return CRC ^ 0xFFFFFFFF;
}
}  // namespace

// Constructors
Message::Message()
{
    Header.MsgId = GenerateMessageId();
    Header.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
}

Message::Message(MessageType MsgType) : Message()
{
    Header.MsgType = MsgType;
}

Message::Message(MessageType MsgType, const std::vector<char>& Payload) : Message(MsgType)
{
    SetPayload(Payload);
}

Message::Message(MessageType MsgType, const std::string& JsonPayload) : Message(MsgType)
{
    SetPayload(JsonPayload);
}

// Copy and move constructors
Message::Message(const Message& Other)
    : Header(Other.Header),
      Payload(Other.Payload),
      IsCompressedFlag(Other.IsCompressedFlag),
      IsEncryptedFlag(Other.IsEncryptedFlag)
{
}

Message::Message(Message&& Other) noexcept
    : Header(std::move(Other.Header)),
      Payload(std::move(Other.Payload)),
      IsCompressedFlag(Other.IsCompressedFlag),
      IsEncryptedFlag(Other.IsEncryptedFlag)
{
    Other.IsCompressedFlag = false;
    Other.IsEncryptedFlag = false;
}

Message& Message::operator=(const Message& Other)
{
    if (this != &Other)
    {
        Header = Other.Header;
        Payload = Other.Payload;
        IsCompressedFlag = Other.IsCompressedFlag;
        IsEncryptedFlag = Other.IsEncryptedFlag;
    }
    return *this;
}

Message& Message::operator=(Message&& Other) noexcept
{
    if (this != &Other)
    {
        Header = std::move(Other.Header);
        Payload = std::move(Other.Payload);
        IsCompressedFlag = Other.IsCompressedFlag;
        IsEncryptedFlag = Other.IsEncryptedFlag;

        Other.IsCompressedFlag = false;
        Other.IsEncryptedFlag = false;
    }
    return *this;
}

// Payload management
void Message::SetPayload(const std::vector<char>& Payload)
{
    this->Payload = Payload;
    UpdateHeaderFromPayload();
}

void Message::SetPayload(const std::string& JsonPayload)
{
    this->Payload.assign(JsonPayload.begin(), JsonPayload.end());
    UpdateHeaderFromPayload();
}

void Message::SetPayload(const char* Data, size_t Size)
{
    this->Payload.assign(Data, Data + Size);
    UpdateHeaderFromPayload();
}

std::string Message::GetJsonPayload() const
{
    if (Payload.empty())
        return "";
    return std::string(reinterpret_cast<const char*>(Payload.data()), Payload.size());
}

bool Message::SetJsonPayload(const std::string& Json)
{
    try
    {
        SetPayload(Json);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// Serialization
std::vector<char> Message::Serialize() const
{
    // Update checksum before serialization
    const_cast<Message*>(this)->UpdateChecksum();

    // Calculate total size
    size_t HeaderSize = sizeof(MessageHeader);
    size_t TotalSize = sizeof(uint32_t) + HeaderSize + Payload.size();  // Magic + Header + Payload

    // Create buffer
    std::vector<char> Buffer(TotalSize);

    // Copy header with magic number
    MessageHeader SerializationHeader = Header;
    uint32_t Magic = MESSAGE_HEADER_MAGIC;

    size_t Offset = 0;
    std::memcpy(Buffer.data() + Offset, &Magic, sizeof(Magic));
    Offset += sizeof(Magic);
    std::memcpy(Buffer.data() + Offset, &SerializationHeader, HeaderSize);
    Offset += HeaderSize;

    // Copy payload
    if (!Payload.empty())
    {
        std::memcpy(Buffer.data() + Offset, Payload.data(), Payload.size());
    }

    return Buffer;
}

bool Message::Deserialize(const std::vector<char>& Data)
{
    return Deserialize(Data.data(), Data.size());
}

bool Message::Deserialize(const char* Data, size_t Size)
{
    // 基本边界：至少包含 magic + header
    if (!Data)
    {
        return false;
    }

    const size_t HeaderSize = sizeof(MessageHeader);
    if (Size < sizeof(uint32_t) + HeaderSize)
    {
        return false;
    }

    size_t Offset = 0;

    // Check magic number
    uint32_t Magic;
    std::memcpy(&Magic, Data + Offset, sizeof(Magic));
    Offset += sizeof(Magic);

    if (Magic != MESSAGE_HEADER_MAGIC)
    {
        return false;
    }

    // Copy header
    std::memcpy(&Header, Data + Offset, HeaderSize);
    Offset += HeaderSize;

    // Copy payload first
    size_t PayloadSize = Size - Offset;
    if (PayloadSize != Header.PayloadSize)
    {
        return false;
    }

    if (PayloadSize > 0)
    {
        Payload.resize(PayloadSize);
        std::memcpy(Payload.data(), Data + Offset, PayloadSize);
    }
    else
    {
        Payload.clear();
    }

    // Validate header after payload is set
    if (!IsValid())
    {
        return false;
    }

    // Validate checksum
    return ValidateChecksum();
}

// Validation
bool Message::IsValid() const
{
    // Check basic header fields
    if (Header.MsgId == InvalidMessageId)
    {
        return false;
    }

    if (Header.PayloadSize != Payload.size())
    {
        return false;
    }

    if (Header.MaxRetries > 10)  // Sanity check
    {
        return false;
    }

    return true;
}

uint32_t Message::CalculateChecksum() const
{
    uint32_t Checksum = 0;

    // Include header fields (excluding checksum field itself)
    Checksum = CalculateCRC32(reinterpret_cast<const char*>(&Header.MsgId), sizeof(Header.MsgId));
    Checksum ^=
        CalculateCRC32(reinterpret_cast<const char*>(&Header.MsgType), sizeof(Header.MsgType));
    Checksum ^=
        CalculateCRC32(reinterpret_cast<const char*>(&Header.SenderId), sizeof(Header.SenderId));
    Checksum ^= CalculateCRC32(reinterpret_cast<const char*>(&Header.ReceiverId),
                               sizeof(Header.ReceiverId));
    Checksum ^= CalculateCRC32(reinterpret_cast<const char*>(&Header.PayloadSize),
                               sizeof(Header.PayloadSize));

    // Include payload
    if (!Payload.empty())
    {
        Checksum ^= CalculateCRC32(Payload.data(), Payload.size());
    }

    return Checksum;
}

bool Message::ValidateChecksum() const
{
    uint32_t CalculatedChecksum = CalculateChecksum();
    return CalculatedChecksum == Header.Checksum;
}

void Message::UpdateChecksum()
{
    Header.Checksum = CalculateChecksum();
}

// Utility functions
size_t Message::GetTotalSize() const
{
    return sizeof(MessageHeader) + Payload.size();
}

void Message::Reset()
{
    Header = MessageHeader{};
    Header.MsgId = GenerateMessageId();
    Header.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    Payload.clear();
    IsCompressedFlag = false;
    IsEncryptedFlag = false;
}

Message Message::Clone() const
{
    return Message(*this);
}

// String representation for debugging
std::string Message::ToString() const
{
    std::ostringstream Oss;
    Oss << "Message{";
    Oss << "Id=" << Header.MsgId;
    Oss << ", Type=" << static_cast<int>(Header.MsgType);
    Oss << ", Sender=" << Header.SenderId;
    Oss << ", Receiver=" << Header.ReceiverId;
    Oss << ", PayloadSize=" << Header.PayloadSize;
    Oss << ", Priority=" << static_cast<int>(Header.Priority);
    Oss << ", Timestamp=" << Header.Timestamp;
    Oss << ", Compressed=" << (IsCompressedFlag ? "true" : "false");
    Oss << ", Encrypted=" << (IsEncryptedFlag ? "true" : "false");
    Oss << "}";
    return Oss.str();
}

std::string Message::GetHeaderString() const
{
    std::ostringstream Oss;
    Oss << "MessageHeader{";
    Oss << "MessageId=" << Header.MsgId;
    Oss << ", MessageType=" << static_cast<int>(Header.MsgType);
    Oss << ", Priority=" << static_cast<int>(Header.Priority);
    Oss << ", DeliveryMode=" << static_cast<int>(Header.PostMode);
    Oss << ", SenderId=" << Header.SenderId;
    Oss << ", ReceiverId=" << Header.ReceiverId;
    Oss << ", TopicId=" << Header.ThemId;
    Oss << ", Timestamp=" << Header.Timestamp;
    Oss << ", PayloadSize=" << Header.PayloadSize;
    Oss << ", Checksum=0x" << std::hex << Header.Checksum;
    Oss << ", SequenceNumber=" << std::dec << Header.SequenceNumber;
    Oss << ", RetryCount=" << Header.RetryCount;
    Oss << ", MaxRetries=" << Header.MaxRetries;
    Oss << ", TimeoutMs=" << Header.TimeoutMs;
    Oss << "}";
    return Oss.str();
}

// Compression (placeholder - would need actual compression library)
bool Message::Compress()
{
    // TODO: Implement actual compression using a library like zlib
    // For now, just mark as compressed
    IsCompressedFlag = true;
    return true;
}

bool Message::Decompress()
{
    // TODO: Implement actual decompression
    // For now, just mark as not compressed
    IsCompressedFlag = false;
    return true;
}

// Encryption (placeholder - would need actual encryption library)
bool Message::Encrypt(const std::string& Key)
{
    // TODO: Implement actual encryption using a library like OpenSSL
    // For now, just mark as encrypted
    IsEncryptedFlag = true;
    return true;
}

bool Message::Decrypt(const std::string& Key)
{
    // TODO: Implement actual decryption
    // For now, just mark as not encrypted
    IsEncryptedFlag = false;
    return true;
}

// Static factory methods
MessagePtr Message::Create(MessageType MsgType)
{
    return std::make_shared<Message>(MsgType);
}

MessagePtr Message::Create(MessageType MsgType, const std::vector<char>& Payload)
{
    return std::make_shared<Message>(MsgType, Payload);
}

MessagePtr Message::Create(MessageType MsgType, const std::string& JsonPayload)
{
    return std::make_shared<Message>(MsgType, JsonPayload);
}

MessagePtr Message::CreateResponse(const Message& OriginalMessage, MessageType ResponseType)
{
    auto Response = std::make_shared<Message>(ResponseType);
    Response->SetReceiverId(OriginalMessage.GetSenderId());
    Response->SetSenderId(OriginalMessage.GetReceiverId());
    Response->SetTopicId(OriginalMessage.GetTopicId());
    return Response;
}

// Private helper methods
void Message::UpdateHeaderFromPayload()
{
    Header.PayloadSize = static_cast<uint32_t>(Payload.size());
}

uint32_t Message::CalculateCRC32(const char* Data, size_t Size) const
{
    return ::Helianthus::Message::CalculateCRC32(Data, Size);
}

}  // namespace Helianthus::Message