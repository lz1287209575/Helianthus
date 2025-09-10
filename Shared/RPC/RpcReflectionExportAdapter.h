#pragma once

#include <string>
#include <vector>

#include "Shared/Reflection/ReflectionExport.h"
#include "RpcReflection.h"

namespace Helianthus::RPC
{

class RpcReflectionExportAdapter : public Helianthus::Reflection::IReflectionExportAdapter
{
public:
    void Begin() override {}

    void OnClass(const Helianthus::Reflection::ClassMeta& Meta) override
    {
        // 确保服务工厂已存在（由反射生成器注入），此处不创建新工厂
        (void)Meta;
    }

    void OnMethod(const std::string& ClassName, const Helianthus::Reflection::MethodMeta& Meta) override
    {
        // 按方法标签在 RPC 注册表登记方法元信息
        RpcMethodMeta M;
        M.MethodName = Meta.Name;
        M.Category = "ReflectedRpc";
        M.RequestTypeName = "";
        M.ResponseTypeName = "";
        M.Tags = Meta.Tags;
        M.Description = "";
        M.Priority = 100;
        RpcServiceRegistry::Get().RegisterMethod(ClassName, M);
    }

    void End() override {}
};

} // namespace Helianthus::RPC


