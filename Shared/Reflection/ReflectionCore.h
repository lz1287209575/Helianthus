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
    std::string Tag;
    std::ptrdiff_t Offset{0};
    std::size_t Size{0};
};

struct MethodMeta
{
    std::string Name;
    std::vector<std::string> Tags;
    std::string ReturnType;
    std::string Visibility; // Public/Protected/Private
    std::string Description;
    bool IsStatic{false};
    std::vector<std::string> ParamNames;
    
    // 新增元数据字段
    bool IsPureFunction{false};     // 纯函数标记
    bool IsConst{false};            // const 方法
    bool IsNoexcept{false};         // noexcept 方法
    bool IsVirtual{false};          // virtual 方法
    bool IsOverride{false};         // override 方法
    bool IsFinal{false};            // final 方法
    bool IsInline{false};           // inline 方法
    bool IsDeprecated{false};       // deprecated 方法
    std::string AccessModifier;     // 访问修饰符
    std::vector<std::string> Qualifiers; // 其他限定符
};

struct ClassMeta
{
    std::string Name;
    std::vector<std::string> Tags;
    std::function<void*()> Factory;
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
        MethodMeta M;
        M.Name = MethodName;
        M.Tags = {Tag};
        M.ReturnType = "";
        M.Visibility = "Public";
        M.Description = "";
        M.IsStatic = false;
        M.ParamNames = {};
        Meta.Methods.push_back(std::move(M));
    }

    void RegisterMethodEx(const std::string& ClassName,
                          const std::string& MethodName,
                          std::vector<std::string> Tags,
                          const std::string& ReturnType,
                          const std::string& Visibility,
                          const std::string& Description,
                          bool IsStatic,
                          std::vector<std::string> ParamNames)
    {
        auto& Meta = Classes[ClassName];
        MethodMeta M;
        M.Name = MethodName;
        M.Tags = std::move(Tags);
        M.ReturnType = ReturnType;
        M.Visibility = Visibility;
        M.Description = Description;
        M.IsStatic = IsStatic;
        M.ParamNames = std::move(ParamNames);
        // 新字段使用默认值
        M.IsPureFunction = false;
        M.IsConst = false;
        M.IsNoexcept = false;
        M.IsVirtual = false;
        M.IsOverride = false;
        M.IsFinal = false;
        M.IsInline = false;
        M.IsDeprecated = false;
        M.AccessModifier = Visibility;
        M.Qualifiers = {};
        Meta.Methods.push_back(std::move(M));
    }

    // 新的完整元数据注册函数
    void RegisterMethodEx(const std::string& ClassName,
                          const std::string& MethodName,
                          std::vector<std::string> Tags,
                          const std::string& ReturnType,
                          const std::string& Visibility,
                          const std::string& Description,
                          bool IsStatic,
                          std::vector<std::string> ParamNames,
                          bool IsPureFunction,
                          bool IsConst,
                          bool IsNoexcept,
                          bool IsVirtual,
                          bool IsOverride,
                          bool IsFinal,
                          bool IsInline,
                          bool IsDeprecated,
                          const std::string& AccessModifier,
                          std::vector<std::string> Qualifiers)
    {
        auto& Meta = Classes[ClassName];
        MethodMeta M;
        M.Name = MethodName;
        M.Tags = std::move(Tags);
        M.ReturnType = ReturnType;
        M.Visibility = Visibility;
        M.Description = Description;
        M.IsStatic = IsStatic;
        M.ParamNames = std::move(ParamNames);
        M.IsPureFunction = IsPureFunction;
        M.IsConst = IsConst;
        M.IsNoexcept = IsNoexcept;
        M.IsVirtual = IsVirtual;
        M.IsOverride = IsOverride;
        M.IsFinal = IsFinal;
        M.IsInline = IsInline;
        M.IsDeprecated = IsDeprecated;
        M.AccessModifier = AccessModifier;
        M.Qualifiers = std::move(Qualifiers);
        Meta.Methods.push_back(std::move(M));
    }
    void AddClassTag(const std::string& ClassName, const std::string& Tag)
    {
        auto& Meta = Classes[ClassName];
        Meta.Name = ClassName;
        Meta.Tags.push_back(Tag);
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


