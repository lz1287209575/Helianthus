#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "RpcTypes.h"

namespace Helianthus::RPC
{
class IRpcServer;  // forward decl for auto-mount API

struct RpcMethodMeta
{
    std::string MethodName;
    std::string Category;           // e.g. "Sync", "Async", "Typed"
    std::string RequestTypeName;    // optional for typed methods
    std::string ResponseTypeName;   // optional for typed methods
    std::vector<std::string> Tags;  // arbitrary labels
    std::string Description;         // human-readable description
    int Priority = 100;              // smaller is higher priority
};

struct RpcServiceMeta
{
    std::string ServiceName;
    std::string Version;
    std::vector<RpcMethodMeta> Methods;
};

class RpcServiceRegistry
{
public:
    using ServiceFactory = std::function<std::shared_ptr<IRpcService>()>;

    static RpcServiceRegistry& Get()
    {
        static RpcServiceRegistry Instance;
        return Instance;
    }

    void RegisterService(const std::string& ServiceName,
                         const std::string& Version,
                         ServiceFactory Factory)
    {
        ServiceFactories[ServiceName] = std::move(Factory);
        ServiceMetas[ServiceName].ServiceName = ServiceName;
        ServiceMetas[ServiceName].Version = Version;
    }

    void RegisterMethod(const std::string& ServiceName, const RpcMethodMeta& Meta)
    {
        ServiceMetas[ServiceName].ServiceName = ServiceName;
        ServiceMetas[ServiceName].Methods.push_back(Meta);
    }

    void ModifyMethodMeta(const std::string& ServiceName,
                          const std::string& MethodName,
                          const std::function<void(RpcMethodMeta&)>& Mutator)
    {
        auto It = ServiceMetas.find(ServiceName);
        if (It == ServiceMetas.end())
        {
            return;
        }
        for (auto& M : It->second.Methods)
        {
            if (M.MethodName == MethodName)
            {
                Mutator(M);
                break;
            }
        }
    }

    std::shared_ptr<IRpcService> Create(const std::string& ServiceName) const
    {
        auto It = ServiceFactories.find(ServiceName);
        if (It == ServiceFactories.end())
        {
            return nullptr;
        }
        return It->second();
    }

    bool HasService(const std::string& ServiceName) const
    {
        return ServiceFactories.find(ServiceName) != ServiceFactories.end();
    }

    RpcServiceMeta GetMeta(const std::string& ServiceName) const
    {
        auto It = ServiceMetas.find(ServiceName);
        return (It != ServiceMetas.end()) ? It->second : RpcServiceMeta{};
    }

    std::vector<std::string> ListServices() const
    {
        std::vector<std::string> Names;
        Names.reserve(ServiceFactories.size());
        for (const auto& [Name, _] : ServiceFactories)
        {
            Names.push_back(Name);
        }
        return Names;
    }

private:
    std::unordered_map<std::string, ServiceFactory> ServiceFactories;
    std::unordered_map<std::string, RpcServiceMeta> ServiceMetas;
};

}  // namespace Helianthus::RPC

namespace Helianthus::RPC
{
// 由代码生成器提供实现：遍历反射服务工厂并注册到给定服务器
void RegisterReflectedServices(IRpcServer& Server);
}


