#include "RpcMessage.h"

#include <cstring>
#include <sstream>

namespace Helianthus::RPC
{
// RpcMessagePayload implementation
std::string RpcMessagePayload::ToJson() const
{
    nlohmann::json Json;
    Json["context"] = {{"call_id", Context.CallId},
                       {"service_id", Context.ServiceIdValue},
                       {"method_id", Context.MethodIdValue},
                       {"service_name", Context.ServiceName},
                       {"method_name", Context.MethodName},
                       {"call_type", static_cast<int>(Context.CallType)},
                       {"format", static_cast<int>(Context.Format)},
                       {"timestamp", Context.Timestamp},
                       {"timeout_ms", Context.TimeoutMs},
                       {"retry_count", Context.RetryCount},
                       {"max_retries", Context.MaxRetries},
                       {"client_id", Context.ClientId},
                       {"server_id", Context.ServerId}};
    Json["parameters"] = Parameters;
    Json["result"] = Result;
    Json["error_code"] = static_cast<int>(ErrorCode);
    Json["error_message"] = ErrorMessage;

    return Json.dump();
}

bool RpcMessagePayload::FromJson(const std::string& JsonStr)
{
    try
    {
        nlohmann::json Json = nlohmann::json::parse(JsonStr);

        if (Json.contains("context"))
        {
            auto& CtxJson = Json["context"];
            Context.CallId = CtxJson.value("call_id", InvalidRpcId);
            Context.ServiceIdValue = CtxJson.value("service_id", InvalidServiceId);
            Context.MethodIdValue = CtxJson.value("method_id", InvalidMethodId);
            Context.ServiceName = CtxJson.value("service_name", "");
            Context.MethodName = CtxJson.value("method_name", "");
            Context.CallType = static_cast<RpcCallType>(CtxJson.value("call_type", 0));
            Context.Format = static_cast<SerializationFormat>(CtxJson.value("format", 0));
            Context.Timestamp = CtxJson.value("timestamp", 0ULL);
            Context.TimeoutMs = CtxJson.value("timeout_ms", 5000U);
            Context.RetryCount = CtxJson.value("retry_count", 0U);
            Context.MaxRetries = CtxJson.value("max_retries", 3U);
            Context.ClientId = CtxJson.value("client_id", "");
            Context.ServerId = CtxJson.value("server_id", "");
        }

        Parameters = Json.value("parameters", "");
        Result = Json.value("result", "");
        ErrorCode = static_cast<RpcResult>(Json.value("error_code", 0));
        ErrorMessage = Json.value("error_message", "");

        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<char> RpcMessagePayload::ToBinary() const
{
    std::vector<char> Buffer;

    // Write context
    Buffer.insert(Buffer.end(),
                  reinterpret_cast<const char*>(&Context.CallId),
                  reinterpret_cast<const char*>(&Context.CallId) + sizeof(Context.CallId));

    // Write strings with length prefix
    auto WriteString = [&Buffer](const std::string& Str)
    {
        uint32_t Length = static_cast<uint32_t>(Str.length());
        Buffer.insert(Buffer.end(),
                      reinterpret_cast<const char*>(&Length),
                      reinterpret_cast<const char*>(&Length) + sizeof(Length));
        Buffer.insert(Buffer.end(), Str.begin(), Str.end());
    };

    WriteString(Context.ServiceName);
    WriteString(Context.MethodName);
    WriteString(Parameters);
    WriteString(Result);
    WriteString(ErrorMessage);

    // Write other fields
    Buffer.insert(Buffer.end(),
                  reinterpret_cast<const char*>(&ErrorCode),
                  reinterpret_cast<const char*>(&ErrorCode) + sizeof(ErrorCode));

    return Buffer;
}

bool RpcMessagePayload::FromBinary(const std::vector<char>& Data)
{
    if (Data.size() < sizeof(RpcId))
        return false;

    const char* Ptr = Data.data();
    size_t Remaining = Data.size();

    // Read context
    if (Remaining < sizeof(Context.CallId))
        return false;
    std::memcpy(&Context.CallId, Ptr, sizeof(Context.CallId));
    Ptr += sizeof(Context.CallId);
    Remaining -= sizeof(Context.CallId);

    // Read strings
    auto ReadString = [&Ptr, &Remaining](std::string& Str) -> bool
    {
        if (Remaining < sizeof(uint32_t))
            return false;
        uint32_t Length;
        std::memcpy(&Length, Ptr, sizeof(Length));
        Ptr += sizeof(Length);
        Remaining -= sizeof(Length);

        if (Remaining < Length)
            return false;
        Str.assign(reinterpret_cast<const char*>(Ptr), Length);
        Ptr += Length;
        Remaining -= Length;
        return true;
    };

    if (!ReadString(Context.ServiceName) || !ReadString(Context.MethodName) ||
        !ReadString(Parameters) || !ReadString(Result) || !ReadString(ErrorMessage))
    {
        return false;
    }

    // Read error code
    if (Remaining < sizeof(ErrorCode))
        return false;
    std::memcpy(&ErrorCode, Ptr, sizeof(ErrorCode));

    return true;
}

// RpcMessage implementation
RpcMessage::RpcMessage()
{
    UpdateUnderlyingMessage();
}

RpcMessage::RpcMessage(const RpcContext& Context)
{
    Payload.Context = Context;
    UpdateUnderlyingMessage();
}

RpcMessage::RpcMessage(const Message::MessagePtr& Msg) : UnderlyingMessage(Msg)
{
    if (Msg)
    {
        FromMessage(Msg);
    }
}

void RpcMessage::SetError(RpcResult Code, const std::string& Message)
{
    Payload.ErrorCode = Code;
    Payload.ErrorMessage = Message;
    Payload.Context.CallType = RpcCallType::ERROR;
    UpdateUnderlyingMessage();
}

Message::MessagePtr RpcMessage::ToMessage() const
{
    auto Msg = Message::Message::Create(Message::MessageType::CUSTOM_MESSAGE_START);

    // Serialize payload based on format
    std::string SerializedPayload;
    switch (Payload.Context.Format)
    {
        case SerializationFormat::JSON:
        {
            SerializedPayload = Payload.ToJson();
            break;
        }
        case SerializationFormat::BINARY:
        {
            auto BinaryData = Payload.ToBinary();
            SerializedPayload.assign(reinterpret_cast<const char*>(BinaryData.data()),
                                     BinaryData.size());
            break;
        }
        default:
            SerializedPayload = Payload.ToJson();
            break;
    }

    Msg->SetPayload(SerializedPayload);
    Msg->SetSenderId(
        static_cast<Common::ServerId>(std::hash<std::string>{}(Payload.Context.ClientId)));
    Msg->SetReceiverId(
        static_cast<Common::ServerId>(std::hash<std::string>{}(Payload.Context.ServerId)));

    return Msg;
}

bool RpcMessage::FromMessage(const Message::MessagePtr& Msg)
{
    if (!Msg)
        return false;

    std::string PayloadStr = Msg->GetJsonPayload();
    if (PayloadStr.empty())
        return false;

    // Try JSON first, then binary
    if (Payload.FromJson(PayloadStr))
    {
        Payload.Context.Format = SerializationFormat::JSON;
        return true;
    }

    // Try binary format
    std::vector<char> BinaryData(PayloadStr.begin(), PayloadStr.end());
    if (Payload.FromBinary(BinaryData))
    {
        Payload.Context.Format = SerializationFormat::BINARY;
        return true;
    }

    return false;
}

std::string RpcMessage::ToString() const
{
    std::ostringstream Oss;
    Oss << "RpcMessage{";
    Oss << "CallId=" << Payload.Context.CallId;
    Oss << ", Service=" << Payload.Context.ServiceName;
    Oss << ", Method=" << Payload.Context.MethodName;
    Oss << ", Type=" << static_cast<int>(Payload.Context.CallType);
    Oss << ", Error=" << static_cast<int>(Payload.ErrorCode);
    if (!Payload.ErrorMessage.empty())
    {
        Oss << ", ErrorMsg=" << Payload.ErrorMessage;
    }
    Oss << "}";
    return Oss.str();
}

size_t RpcMessage::GetSerializedSize() const
{
    switch (Payload.Context.Format)
    {
        case SerializationFormat::JSON:
            return Payload.ToJson().size();
        case SerializationFormat::BINARY:
            return Payload.ToBinary().size();
        default:
            return Payload.ToJson().size();
    }
}

void RpcMessage::UpdateUnderlyingMessage()
{
    UnderlyingMessage = ToMessage();
}


}  // namespace Helianthus::RPC