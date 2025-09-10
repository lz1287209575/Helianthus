#pragma once

#include <string>
#include <vector>

#include "ReflectionCore.h"

namespace Helianthus::Reflection
{

class IReflectionExportAdapter
{
public:
    virtual ~IReflectionExportAdapter() = default;

    virtual void Begin() {}
    virtual void OnClass(const ClassMeta& Meta) { (void)Meta; }
    virtual void OnMethod(const std::string& ClassName, const MethodMeta& Meta) { (void)ClassName; (void)Meta; }
    virtual void End() {}
};

// 统一遍历注册表并通过适配器导出
inline void ExportReflection(IReflectionExportAdapter& Adapter)
{
    Adapter.Begin();
    const auto Names = ClassRegistry::Get().List();
    for (const auto& Name : Names)
    {
        const ClassMeta* Meta = ClassRegistry::Get().Get(Name);
        if (!Meta) { continue; }
        Adapter.OnClass(*Meta);
        for (const auto& M : Meta->Methods)
        {
            Adapter.OnMethod(Meta->Name, M);
        }
    }
    Adapter.End();
}

} // namespace Helianthus::Reflection


