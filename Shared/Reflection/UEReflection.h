#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Helianthus::Reflection
{

struct PropertyMeta
{
    std::string Name;
    std::string Tag;     // e.g. ScriptReadable
    std::ptrdiff_t Offset{0};
    std::size_t Size{0};
};

struct MethodMeta
{
    std::string Name;
    std::string Tag;     // e.g. ScriptCallable / ScriptImpl / Rpc
};

struct ClassMeta
{
    std::string Name;
    std::vector<std::string> Tags;  // e.g. ScriptCreateable
    std::function<void*()> Factory; // optional; returns new Class instance
    std::vector<PropertyMeta> Properties;
    std::vector<MethodMeta> Methods;
};

class ClassRegistry
{
public:
    static ClassRegistry& Get()
    {
        static ClassRegistry Instance;
        return Instance;
    }

    void RegisterClass(const std::string& ClassName,
                       std::vector<std::string> Tags,
                       std::function<void*()> Factory)
    {
        auto& Meta = Classes[ClassName];
        Meta.Name = ClassName;
        Meta.Tags = std::move(Tags);
        Meta.Factory = std::move(Factory);
    }

    void RegisterProperty(const std::string& ClassName,
                          const std::string& PropName,
                          const std::string& Tag,
                          std::ptrdiff_t Offset,
                          std::size_t Size)
    {
        auto& Meta = Classes[ClassName];
        Meta.Properties.push_back(PropertyMeta{PropName, Tag, Offset, Size});
    }

    void RegisterMethod(const std::string& ClassName,
                        const std::string& MethodName,
                        const std::string& Tag)
    {
        auto& Meta = Classes[ClassName];
        Meta.Methods.push_back(MethodMeta{MethodName, Tag});
    }

    bool Has(const std::string& ClassName) const
    {
        return Classes.find(ClassName) != Classes.end();
    }

    const ClassMeta* Get(const std::string& ClassName) const
    {
        auto It = Classes.find(ClassName);
        return (It != Classes.end()) ? &It->second : nullptr;
    }

    void* Create(const std::string& ClassName) const
    {
        auto It = Classes.find(ClassName);
        if (It == Classes.end() || !It->second.Factory)
        {
            return nullptr;
        }
        return It->second.Factory();
    }

    std::vector<std::string> List() const
    {
        std::vector<std::string> Names;
        Names.reserve(Classes.size());
        for (const auto& [Name, _] : Classes)
        {
            Names.push_back(Name);
        }
        return Names;
    }

private:
    std::unordered_map<std::string, ClassMeta> Classes;
};

}  // namespace Helianthus::Reflection


