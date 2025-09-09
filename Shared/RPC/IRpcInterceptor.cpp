#include "IRpcInterceptor.h"
#include <algorithm>
#include "Shared/Common/LogCategories.h"

namespace Helianthus::RPC
{

void RpcInterceptorChain::AddInterceptor(RpcInterceptorPtr Interceptor)
{
    if (!Interceptor)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "Attempted to add null interceptor");
        return;
    }

    std::lock_guard<std::mutex> Lock(InterceptorsMutex);
    
    // Check if interceptor with same name already exists
    auto It = std::find_if(Interceptors.begin(), Interceptors.end(),
        [&Interceptor](const RpcInterceptorPtr& Existing)
        {
            return Existing->GetName() == Interceptor->GetName();
        });

    if (It != Interceptors.end())
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "Interceptor with name '{}' already exists, replacing", Interceptor->GetName());
        *It = Interceptor;
    }
    else
    {
        Interceptors.push_back(Interceptor);
    }

    SortInterceptors();
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Added interceptor '{}' with priority {}", Interceptor->GetName(), Interceptor->GetPriority());
}

void RpcInterceptorChain::RemoveInterceptor(const std::string& Name)
{
    std::lock_guard<std::mutex> Lock(InterceptorsMutex);
    
    auto It = std::find_if(Interceptors.begin(), Interceptors.end(),
        [&Name](const RpcInterceptorPtr& Interceptor)
        {
            return Interceptor->GetName() == Name;
        });

    if (It != Interceptors.end())
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Removing interceptor '{}'", Name);
        Interceptors.erase(It);
    }
    else
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "Interceptor '{}' not found for removal", Name);
    }
}

void RpcInterceptorChain::Clear()
{
    std::lock_guard<std::mutex> Lock(InterceptorsMutex);
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Clearing all interceptors (count: {})", Interceptors.size());
    Interceptors.clear();
}

RpcResult RpcInterceptorChain::Execute(RpcContext& Context, const RpcMessage& Message, 
                                      std::function<RpcResult(const RpcMessage&)> Handler)
{
    std::vector<RpcInterceptorPtr> LocalInterceptors;
    
    // Copy interceptors to avoid holding lock during execution
    {
        std::lock_guard<std::mutex> Lock(InterceptorsMutex);
        LocalInterceptors = Interceptors;
    }

    // Execute before-call interceptors
    for (const auto& Interceptor : LocalInterceptors)
    {
        try
        {
            if (!Interceptor->OnBeforeCall(Context, Message))
            {
                H_LOG(Rpc, Helianthus::Common::LogVerbosity::Verbose, "Interceptor '{}' blocked RPC call", Interceptor->GetName());
                return RpcResult::INTERNAL_ERROR;
            }
        }
        catch (const std::exception& Ex)
        {
            H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "Interceptor '{}' threw exception in OnBeforeCall: {}", 
                         Interceptor->GetName(), Ex.what());
            Interceptor->OnError(Context, Message, Ex.what());
            return RpcResult::INTERNAL_ERROR;
        }
    }

    // Execute the actual RPC call
    RpcResult Result;
    try
    {
        Result = Handler(Message);
    }
    catch (const std::exception& Ex)
    {
        H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "RPC handler threw exception: {}", Ex.what());
        Result = RpcResult::INTERNAL_ERROR;
    }

    // Execute after-call interceptors (in reverse order)
    for (auto It = LocalInterceptors.rbegin(); It != LocalInterceptors.rend(); ++It)
    {
        try
        {
            if (Result == RpcResult::SUCCESS)
            {
                (*It)->OnAfterCall(Context, Message, Result);
            }
            else
            {
                (*It)->OnError(Context, Message, "RPC call failed");
            }
        }
        catch (const std::exception& Ex)
        {
            H_LOG(Rpc, Helianthus::Common::LogVerbosity::Error, "Interceptor '{}' threw exception in OnAfterCall/OnError: {}", 
                         (*It)->GetName(), Ex.what());
        }
    }

    return Result;
}

size_t RpcInterceptorChain::GetInterceptorCount() const
{
    std::lock_guard<std::mutex> Lock(InterceptorsMutex);
    return Interceptors.size();
}

bool RpcInterceptorChain::HasInterceptor(const std::string& Name) const
{
    std::lock_guard<std::mutex> Lock(InterceptorsMutex);
    
    return std::any_of(Interceptors.begin(), Interceptors.end(),
        [&Name](const RpcInterceptorPtr& Interceptor)
        {
            return Interceptor->GetName() == Name;
        });
}

void RpcInterceptorChain::SortInterceptors()
{
    std::sort(Interceptors.begin(), Interceptors.end(),
        [](const RpcInterceptorPtr& A, const RpcInterceptorPtr& B)
        {
            return A->GetPriority() < B->GetPriority();
        });
}

} // namespace Helianthus::RPC
