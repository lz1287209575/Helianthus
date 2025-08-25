#include "ReflectionTypes.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace Helianthus::Reflection
{
    // 全局反射系统实例
    std::unique_ptr<ReflectionSystem> GlobalReflectionSystem;

    // 反射系统实现
    void ReflectionSystem::RegisterClass(const ClassInfo& ClassInfo)
    {
        Classes[ClassInfo.Name] = ClassInfo;
        TypeToClass[ClassInfo.TypeIndex] = &Classes[ClassInfo.Name];
    }

    void ReflectionSystem::RegisterEnum(const EnumInfo& EnumInfo)
    {
        Enums[EnumInfo.Name] = EnumInfo;
    }

    const ClassInfo* ReflectionSystem::GetClassInfo(const std::string& ClassName) const
    {
        auto It = Classes.find(ClassName);
        return (It != Classes.end()) ? &It->second : nullptr;
    }

    const ClassInfo* ReflectionSystem::GetClassInfo(const std::type_index& TypeIndex) const
    {
        auto It = TypeToClass.find(TypeIndex);
        return (It != TypeToClass.end()) ? It->second : nullptr;
    }

    const EnumInfo* ReflectionSystem::GetEnumInfo(const std::string& EnumName) const
    {
        auto It = Enums.find(EnumName);
        return (It != Enums.end()) ? &It->second : nullptr;
    }

    std::vector<std::string> ReflectionSystem::GetAllClassNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(Classes.size());
        for (const auto& Pair : Classes)
        {
            Names.push_back(Pair.first);
        }
        return Names;
    }

    std::vector<std::string> ReflectionSystem::GetAllEnumNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(Enums.size());
        for (const auto& Pair : Enums)
        {
            Names.push_back(Pair.first);
        }
        return Names;
    }

    void* ReflectionSystem::CreateObject(const std::string& ClassName)
    {
        const ClassInfo* Info = GetClassInfo(ClassName);
        if (!Info)
        {
            throw std::runtime_error("Class not found: " + ClassName);
        }

        if (!Info->Constructor)
        {
            throw std::runtime_error("No constructor available for class: " + ClassName);
        }

        return Info->Constructor(nullptr);
    }

    void ReflectionSystem::DestroyObject(const std::string& ClassName, void* Object)
    {
        const ClassInfo* Info = GetClassInfo(ClassName);
        if (!Info)
        {
            throw std::runtime_error("Class not found: " + ClassName);
        }

        if (!Info->Destructor)
        {
            throw std::runtime_error("No destructor available for class: " + ClassName);
        }

        Info->Destructor(Object);
    }

    void* ReflectionSystem::GetProperty(void* Object, const std::string& PropertyName)
    {
        // 这里需要根据对象类型查找对应的类信息
        // 简化实现，假设对象有类型信息
        for (const auto& Pair : Classes)
        {
            const ClassInfo& ClassInfo = Pair.second;
            for (const auto& Property : ClassInfo.Properties)
            {
                if (Property.Name == PropertyName && Property.Getter)
                {
                    return Property.Getter(Object);
                }
            }
        }
        return nullptr;
    }

    void ReflectionSystem::SetProperty(void* Object, const std::string& PropertyName, void* Value)
    {
        for (const auto& Pair : Classes)
        {
            const ClassInfo& ClassInfo = Pair.second;
            for (const auto& Property : ClassInfo.Properties)
            {
                if (Property.Name == PropertyName && Property.Setter)
                {
                    Property.Setter(Object, Value);
                    return;
                }
            }
        }
    }

    void* ReflectionSystem::CallMethod(void* Object, const std::string& MethodName, 
                                      const std::vector<void*>& Arguments)
    {
        for (const auto& Pair : Classes)
        {
            const ClassInfo& ClassInfo = Pair.second;
            for (const auto& Method : ClassInfo.Methods)
            {
                if (Method.Name == MethodName && Method.Invoker)
                {
                    return Method.Invoker(Object, Arguments);
                }
            }
        }
        return nullptr;
    }

    bool ReflectionSystem::IsInstanceOf(void* Object, const std::string& ClassName) const
    {
        // 简化实现，实际需要运行时类型信息
        return GetClassInfo(ClassName) != nullptr;
    }

    bool ReflectionSystem::IsSubclassOf(const std::string& ClassName, const std::string& BaseClassName) const
    {
        const ClassInfo* ClassInfo = GetClassInfo(ClassName);
        if (!ClassInfo)
        {
            return false;
        }

        // 检查直接基类
        if (ClassInfo->BaseClassName == BaseClassName)
        {
            return true;
        }

        // 递归检查基类的基类
        return IsSubclassOf(ClassInfo->BaseClassName, BaseClassName);
    }

    // 反射系统初始化
    void InitializeReflectionSystem()
    {
        if (!GlobalReflectionSystem)
        {
            GlobalReflectionSystem = std::make_unique<ReflectionSystem>();
        }
    }

    void ShutdownReflectionSystem()
    {
        GlobalReflectionSystem.reset();
    }

} // namespace Helianthus::Reflection
