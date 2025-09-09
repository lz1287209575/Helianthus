#include "RpcMessageHandler.h"

#include <chrono>

namespace Helianthus::RPC
{

RpcMessageHandler::RpcMessageHandler()
    : DefaultFormat(SerializationFormat::JSON)
{
    // Initialize default serializers
    SetSerializer(SerializationFormat::JSON, RpcSerializerFactory::CreateSerializer(SerializationFormat::JSON));
    SetSerializer(SerializationFormat::BINARY, RpcSerializerFactory::CreateSerializer(SerializationFormat::BINARY));
}

RpcMessageHandler::RpcMessageHandler(SerializationFormat DefaultFormat)
    : DefaultFormat(DefaultFormat)
{
    // Initialize default serializers
    SetSerializer(SerializationFormat::JSON, RpcSerializerFactory::CreateSerializer(SerializationFormat::JSON));
    SetSerializer(SerializationFormat::BINARY, RpcSerializerFactory::CreateSerializer(SerializationFormat::BINARY));
}

RpcResult RpcMessageHandler::ProcessMessage(const RpcMessage& Message, RpcMessage& Response)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    // Validate message
    RpcResult ValidationResult = ValidateMessage(Message);
    if (ValidationResult != RpcResult::SUCCESS)
    {
        UpdateStats(Message.GetContext(), ValidationResult, 0);
        return ValidationResult;
    }

    // Apply middleware
    RpcContext Context = Message.GetContext();
    if (!ApplyMiddleware(Context))
    {
        UpdateStats(Context, RpcResult::INTERNAL_ERROR, 0);
        return RpcResult::INTERNAL_ERROR;
    }

    // Execute interceptors and actual processing
    return InterceptorChain.Execute(Context, Message, [this, &Response, &Context, &Message, StartTime](const RpcMessage& Msg) -> RpcResult {
        // Process based on call type
        RpcResult Result = RpcResult::SUCCESS;
        std::string ResponseData;

        switch (Context.CallType)
        {
            case RpcCallType::REQUEST:
                Result = ProcessRequest(Context, Message.GetParameters(), ResponseData);
                break;
            case RpcCallType::NOTIFICATION:
                // One-way call, no response needed
                Result = ProcessRequest(Context, Message.GetParameters(), ResponseData);
                return Result;  // Don't send response for notifications
            case RpcCallType::HEARTBEAT:
                ResponseData = "pong";
                break;
            default:
                Result = RpcResult::INVALID_PARAMETERS;
                break;
        }

        // Calculate latency
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto LatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime).count();

        // Update statistics
        UpdateStats(Context, Result, LatencyMs);

        // Prepare response
        if (Result == RpcResult::SUCCESS && Context.CallType == RpcCallType::REQUEST)
        {
            Response.SetContext(Context);
            Context.CallType = RpcCallType::RESPONSE;
            Response.SetContext(Context);
            Response.SetResult(ResponseData);
        }
        else if (Result != RpcResult::SUCCESS)
        {
            Response.SetContext(Context);
            Context.CallType = RpcCallType::ERROR;
            Response.SetContext(Context);
            Response.SetError(Result, "Error: " + std::to_string(static_cast<int>(Result)));
        }

        return Result;
    });
}

RpcResult RpcMessageHandler::ProcessRequest(const RpcContext& Context, const std::string& Parameters, std::string& Result)
{
    return RouteToService(Context, Parameters, Result);
}

RpcResult RpcMessageHandler::ProcessResponse(const RpcMessage& Message)
{
    // Handle response message (typically for async calls)
    // This would be implemented based on the specific async handling requirements
    return RpcResult::SUCCESS;
}

void RpcMessageHandler::RegisterService(RpcServicePtr Service)
{
    if (!Service)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(ServicesMutex);
    Services[Service->GetServiceName()] = Service;
}

void RpcMessageHandler::UnregisterService(const std::string& ServiceName)
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);
    Services.erase(ServiceName);
}

RpcServicePtr RpcMessageHandler::GetService(const std::string& ServiceName) const
{
    std::lock_guard<std::mutex> Lock(ServicesMutex);
    auto It = Services.find(ServiceName);
    return (It != Services.end()) ? It->second : nullptr;
}

void RpcMessageHandler::SetDefaultFormat(SerializationFormat Format)
{
    DefaultFormat = Format;
}

SerializationFormat RpcMessageHandler::GetDefaultFormat() const
{
    return DefaultFormat;
}

void RpcMessageHandler::SetSerializer(SerializationFormat Format, std::unique_ptr<IRpcSerializer> Serializer)
{
    std::lock_guard<std::mutex> Lock(SerializersMutex);
    Serializers[Format] = std::move(Serializer);
}

IRpcSerializer* RpcMessageHandler::GetSerializer(SerializationFormat Format) const
{
    std::lock_guard<std::mutex> Lock(SerializersMutex);
    auto It = Serializers.find(Format);
    return (It != Serializers.end()) ? It->second.get() : nullptr;
}

void RpcMessageHandler::AddMiddleware(std::function<bool(RpcContext&)> Middleware)
{
    std::lock_guard<std::mutex> Lock(MiddlewareMutex);
    MiddlewareChain.push_back(Middleware);
}

void RpcMessageHandler::ClearMiddleware()
{
    std::lock_guard<std::mutex> Lock(MiddlewareMutex);
    MiddlewareChain.clear();
}

bool RpcMessageHandler::ApplyMiddleware(RpcContext& Context)
{
    std::lock_guard<std::mutex> Lock(MiddlewareMutex);
    
    for (const auto& Middleware : MiddlewareChain)
    {
        if (!Middleware(Context))
        {
            return false;
        }
    }
    
    return true;
}

RpcStats RpcMessageHandler::GetStats() const
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    return Stats;
}

void RpcMessageHandler::ResetStats()
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    Stats = RpcStats{};
}

void RpcMessageHandler::UpdateStats(const RpcContext& Context, RpcResult Result, uint64_t LatencyMs)
{
    std::lock_guard<std::mutex> Lock(StatsMutex);
    
    Stats.TotalCalls++;
    
    if (Result == RpcResult::SUCCESS)
    {
        Stats.SuccessfulCalls++;
    }
    else
    {
        Stats.FailedCalls++;
        if (Result == RpcResult::TIMEOUT)
        {
            Stats.TimeoutCalls++;
        }
    }
    
    // Update latency statistics
    if (LatencyMs > 0)
    {
        if (Stats.MinLatencyMs == 0 || LatencyMs < Stats.MinLatencyMs)
        {
            Stats.MinLatencyMs = LatencyMs;
        }
        if (LatencyMs > Stats.MaxLatencyMs)
        {
            Stats.MaxLatencyMs = LatencyMs;
        }
        
        // Update average latency
        Stats.AverageLatencyMs = (Stats.AverageLatencyMs * (Stats.TotalCalls - 1) + LatencyMs) / Stats.TotalCalls;
    }
}

void RpcMessageHandler::SetErrorHandler(RpcErrorHandler Handler)
{
    std::lock_guard<std::mutex> Lock(ErrorHandlerMutex);
    ErrorHandler = Handler;
}

void RpcMessageHandler::HandleError(RpcResult Error, const std::string& Message)
{
    std::lock_guard<std::mutex> Lock(ErrorHandlerMutex);
    if (ErrorHandler)
    {
        ErrorHandler(Error, Message);
    }
}

RpcResult RpcMessageHandler::ValidateMessage(const RpcMessage& Message) const
{
    if (Message.GetContext().ServiceName.empty())
    {
        return RpcResult::INVALID_PARAMETERS;
    }
    
    if (Message.GetContext().MethodName.empty())
    {
        return RpcResult::INVALID_PARAMETERS;
    }
    
    return RpcResult::SUCCESS;
}

RpcResult RpcMessageHandler::RouteToService(const RpcContext& Context, const std::string& Parameters, std::string& Result)
{
    RpcServicePtr Service = GetService(Context.ServiceName);
    if (!Service)
    {
        return RpcResult::SERVICE_NOT_FOUND;
    }
    
    return Service->HandleCall(Context, Context.MethodName, Parameters, Result);
}

std::string RpcMessageHandler::SerializeResponse(const RpcContext& Context, const std::string& Result) const
{
    IRpcSerializer* Serializer = GetSerializer(Context.Format);
    if (!Serializer)
    {
        Serializer = GetSerializer(DefaultFormat);
    }
    
    if (Serializer)
    {
        return Serializer->Serialize(&Result, "string");
    }
    
    return Result;
}

bool RpcMessageHandler::DeserializeRequest(const std::string& Data, const RpcContext& Context, std::string& Parameters) const
{
    IRpcSerializer* Serializer = GetSerializer(Context.Format);
    if (!Serializer)
    {
        Serializer = GetSerializer(DefaultFormat);
    }
    
    if (Serializer)
    {
        return Serializer->Deserialize(Data, &Parameters, "string");
    }
    
    Parameters = Data;
    return true;
}

// Interceptor support methods
void RpcMessageHandler::AddInterceptor(RpcInterceptorPtr Interceptor)
{
    InterceptorChain.AddInterceptor(Interceptor);
}

void RpcMessageHandler::RemoveInterceptor(const std::string& Name)
{
    InterceptorChain.RemoveInterceptor(Name);
}

void RpcMessageHandler::ClearInterceptors()
{
    InterceptorChain.Clear();
}

RpcInterceptorChain& RpcMessageHandler::GetInterceptorChain()
{
    return InterceptorChain;
}

}  // namespace Helianthus::RPC
