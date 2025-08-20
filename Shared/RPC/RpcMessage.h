#pragma once

#include "Shared/Message/Message.h"

#include "RpcTypes.h"
#include <nlohmann/json.hpp>

namespace Helianthus::RPC
{
/**
 * @brief RPC message payload structure
 *
 * This represents the data structure sent over the wire for RPC calls.
 * It can be serialized to JSON, binary, or other formats.
 */
struct RpcMessagePayload
{
    RpcContext Context;
    std::string Parameters;  // Serialized parameters
    std::string Result;      // Serialized result (for responses)
    RpcResult ErrorCode = RpcResult::SUCCESS;
    std::string ErrorMessage;

    // Serialization methods
    std::string ToJson() const;
    bool FromJson(const std::string& Json);

    std::vector<char> ToBinary() const;
    bool FromBinary(const std::vector<char>& Data);
};

/**
 * @brief High-level RPC message wrapper
 *
 * Wraps the low-level Message class with RPC-specific functionality
 */
class RpcMessage
{
public:
    RpcMessage();
    explicit RpcMessage(const RpcContext& Context);
    explicit RpcMessage(const Message::MessagePtr& Msg);

    // Accessors
    RpcContext GetContext() const
    {
        return Payload.Context;
    }
    void SetContext(const RpcContext& Context)
    {
        Payload.Context = Context;
    }

    const std::string& GetParameters() const
    {
        return Payload.Parameters;
    }
    void SetParameters(const std::string& Parameters)
    {
        Payload.Parameters = Parameters;
    }

    const std::string& GetResult() const
    {
        return Payload.Result;
    }
    void SetResult(const std::string& Result)
    {
        Payload.Result = Result;
    }

    RpcResult GetErrorCode() const
    {
        return Payload.ErrorCode;
    }
    void SetError(RpcResult Code, const std::string& Message);

    // Type checking
    bool IsRequest() const
    {
        return Payload.Context.CallType == RpcCallType::REQUEST;
    }
    bool IsResponse() const
    {
        return Payload.Context.CallType == RpcCallType::RESPONSE;
    }
    bool IsNotification() const
    {
        return Payload.Context.CallType == RpcCallType::NOTIFICATION;
    }
    bool IsError() const
    {
        return Payload.ErrorCode != RpcResult::SUCCESS;
    }

    // Serialization
    Message::MessagePtr ToMessage() const;
    bool FromMessage(const Message::MessagePtr& Msg);

    // Utility
    std::string ToString() const;
    size_t GetSerializedSize() const;

private:
    RpcMessagePayload Payload;
    Message::MessagePtr UnderlyingMessage;

    void UpdateUnderlyingMessage();
};

/**
 * @brief RPC serializer interface
 */
class IRpcSerializer
{
public:
    virtual ~IRpcSerializer() = default;

    virtual std::string Serialize(const RpcMessagePayload& Payload) = 0;
    virtual bool Deserialize(const std::string& Data, RpcMessagePayload& Payload) = 0;
    virtual SerializationFormat GetFormat() const = 0;
};

/**
 * @brief JSON RPC serializer
 */
class JsonRpcSerializer : public IRpcSerializer
{
public:
    std::string Serialize(const RpcMessagePayload& Payload) override;
    bool Deserialize(const std::string& Data, RpcMessagePayload& Payload) override;
    SerializationFormat GetFormat() const override
    {
        return SerializationFormat::JSON;
    }

private:
    nlohmann::json ContextToJson(const RpcContext& Context) const;
    RpcContext JsonToContext(const nlohmann::json& Json) const;
};

/**
 * @brief Binary RPC serializer (simple implementation)
 */
class BinaryRpcSerializer : public IRpcSerializer
{
public:
    std::string Serialize(const RpcMessagePayload& Payload) override;
    bool Deserialize(const std::string& Data, RpcMessagePayload& Payload) override;
    SerializationFormat GetFormat() const override
    {
        return SerializationFormat::BINARY;
    }

private:
    void WriteString(std::vector<char>& Buffer, const std::string& Str) const;
    std::string ReadString(const char*& Data, size_t& Remaining) const;
};

/**
 * @brief Serializer factory
 */
class RpcSerializerFactory
{
public:
    static std::unique_ptr<IRpcSerializer> Create(SerializationFormat Format);
    static std::unique_ptr<IRpcSerializer> CreateJson();
    static std::unique_ptr<IRpcSerializer> CreateBinary();
};

}  // namespace Helianthus::RPC