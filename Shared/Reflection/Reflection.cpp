#include "Reflection.h"
#include <iostream>
#include <algorithm>

namespace Helianthus::Reflection
{
    // 全局实例
    HelianthusReflectionSystem* GHelianthusReflectionSystem = nullptr;

    // 单例实现
    HelianthusReflectionSystem& HelianthusReflectionSystem::GetInstance()
    {
        static HelianthusReflectionSystem Instance;
        return Instance;
    }

    // 注册方法
    void HelianthusReflectionSystem::RegisterHClass(const HClassInfo& ClassInfo)
    {
        HClasses[ClassInfo.ClassName] = ClassInfo;
        std::cout << "注册类: " << ClassInfo.ClassName << std::endl;
    }

    void HelianthusReflectionSystem::RegisterHProperty(const std::string& ClassName, const HPropertyInfo& PropertyInfo)
    {
        HProperties[ClassName][PropertyInfo.PropertyName] = PropertyInfo;
        std::cout << "注册属性: " << ClassName << "::" << PropertyInfo.PropertyName << std::endl;
    }

    void HelianthusReflectionSystem::RegisterHFunction(const std::string& ClassName, const HFunctionInfo& FunctionInfo)
    {
        HFunctions[ClassName][FunctionInfo.FunctionName] = FunctionInfo;
        std::cout << "注册方法: " << ClassName << "::" << FunctionInfo.FunctionName << std::endl;
    }

    void HelianthusReflectionSystem::RegisterHEnum(const HEnumInfo& EnumInfo)
    {
        HEnums[EnumInfo.EnumName] = EnumInfo;
        std::cout << "注册枚举: " << EnumInfo.EnumName << std::endl;
    }

    // 查询方法
    const HClassInfo* HelianthusReflectionSystem::GetHClassInfo(const std::string& ClassName) const
    {
        auto It = HClasses.find(ClassName);
        if (It == HClasses.end()) return nullptr;
        
        // 创建完整的类信息，包含属性和方法
        static HClassInfo CompleteClassInfo;
        CompleteClassInfo = It->second;
        
        // 添加属性
        auto PropertyIt = HProperties.find(ClassName);
        if (PropertyIt != HProperties.end())
        {
            for (const auto& PropertyPair : PropertyIt->second)
            {
                CompleteClassInfo.Properties.push_back(PropertyPair.second);
            }
        }
        
        // 添加方法
        auto FunctionIt = HFunctions.find(ClassName);
        if (FunctionIt != HFunctions.end())
        {
            for (const auto& FunctionPair : FunctionIt->second)
            {
                CompleteClassInfo.Functions.push_back(FunctionPair.second);
            }
        }
        
        return &CompleteClassInfo;
    }

    const HPropertyInfo* HelianthusReflectionSystem::GetHPropertyInfo(const std::string& ClassName, const std::string& PropertyName) const
    {
        auto ClassIt = HProperties.find(ClassName);
        if (ClassIt == HProperties.end()) return nullptr;
        
        auto PropertyIt = ClassIt->second.find(PropertyName);
        return (PropertyIt != ClassIt->second.end()) ? &PropertyIt->second : nullptr;
    }

    const HFunctionInfo* HelianthusReflectionSystem::GetHFunctionInfo(const std::string& ClassName, const std::string& FunctionName) const
    {
        auto ClassIt = HFunctions.find(ClassName);
        if (ClassIt == HFunctions.end()) return nullptr;
        
        auto FunctionIt = ClassIt->second.find(FunctionName);
        return (FunctionIt != ClassIt->second.end()) ? &FunctionIt->second : nullptr;
    }

    const HEnumInfo* HelianthusReflectionSystem::GetHEnumInfo(const std::string& EnumName) const
    {
        auto It = HEnums.find(EnumName);
        return (It != HEnums.end()) ? &It->second : nullptr;
    }

    // 对象操作方法
    void* HelianthusReflectionSystem::CreateHObject(const std::string& ClassName)
    {
        const HClassInfo* ClassInfo = GetHClassInfo(ClassName);
        if (!ClassInfo || !ClassInfo->Constructor) return nullptr;
        
        return ClassInfo->Constructor(nullptr);
    }

    void HelianthusReflectionSystem::DestroyHObject(const std::string& ClassName, void* Object)
    {
        const HClassInfo* ClassInfo = GetHClassInfo(ClassName);
        if (!ClassInfo || !ClassInfo->Destructor) return;
        
        ClassInfo->Destructor(Object);
    }

    void* HelianthusReflectionSystem::GetHProperty(void* Object, const std::string& PropertyName)
    {
        // 简化实现，实际需要根据对象类型查找对应的属性
        return nullptr;
    }

    void HelianthusReflectionSystem::SetHProperty(void* Object, const std::string& PropertyName, void* Value)
    {
        // 简化实现，实际需要根据对象类型查找对应的属性
    }

    void* HelianthusReflectionSystem::CallHFunction(void* Object, const std::string& FunctionName, const std::vector<void*>& Arguments)
    {
        // 简化实现，实际需要根据对象类型查找对应的方法
        return nullptr;
    }

    // 类型检查方法
    bool HelianthusReflectionSystem::IsHClass(const std::string& ClassName) const
    {
        return HClasses.find(ClassName) != HClasses.end();
    }

    bool HelianthusReflectionSystem::IsHProperty(const std::string& ClassName, const std::string& PropertyName) const
    {
        auto ClassIt = HProperties.find(ClassName);
        if (ClassIt == HProperties.end()) return false;
        
        return ClassIt->second.find(PropertyName) != ClassIt->second.end();
    }

    bool HelianthusReflectionSystem::IsHFunction(const std::string& ClassName, const std::string& FunctionName) const
    {
        auto ClassIt = HFunctions.find(ClassName);
        if (ClassIt == HFunctions.end()) return false;
        
        return ClassIt->second.find(FunctionName) != ClassIt->second.end();
    }

    bool HelianthusReflectionSystem::IsHEnum(const std::string& EnumName) const
    {
        return HEnums.find(EnumName) != HEnums.end();
    }

    // 枚举方法
    std::vector<std::string> HelianthusReflectionSystem::GetAllHClassNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(HClasses.size());
        
        for (const auto& Pair : HClasses)
        {
            Names.push_back(Pair.first);
        }
        
        return Names;
    }

    std::vector<std::string> HelianthusReflectionSystem::GetAllHPropertyNames(const std::string& ClassName) const
    {
        std::vector<std::string> Names;
        auto ClassIt = HProperties.find(ClassName);
        
        if (ClassIt != HProperties.end())
        {
            Names.reserve(ClassIt->second.size());
            for (const auto& Pair : ClassIt->second)
            {
                Names.push_back(Pair.first);
            }
        }
        
        return Names;
    }

    std::vector<std::string> HelianthusReflectionSystem::GetAllHFunctionNames(const std::string& ClassName) const
    {
        std::vector<std::string> Names;
        auto ClassIt = HFunctions.find(ClassName);
        
        if (ClassIt != HFunctions.end())
        {
            Names.reserve(ClassIt->second.size());
            for (const auto& Pair : ClassIt->second)
            {
                Names.push_back(Pair.first);
            }
        }
        
        return Names;
    }

    std::vector<std::string> HelianthusReflectionSystem::GetAllHEnumNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(HEnums.size());
        
        for (const auto& Pair : HEnums)
        {
            Names.push_back(Pair.first);
        }
        
        return Names;
    }

    // 统计方法
    size_t HelianthusReflectionSystem::GetRegisteredHClassCount() const
    {
        return HClasses.size();
    }

    size_t HelianthusReflectionSystem::GetRegisteredHEnumCount() const
    {
        return HEnums.size();
    }

    // 获取所有信息方法
    std::vector<HClassInfo> HelianthusReflectionSystem::GetAllHClassInfos() const
    {
        std::vector<HClassInfo> Infos;
        Infos.reserve(HClasses.size());
        
        for (const auto& Pair : HClasses)
        {
            Infos.push_back(Pair.second);
        }
        
        return Infos;
    }

    std::vector<HEnumInfo> HelianthusReflectionSystem::GetAllHEnumInfos() const
    {
        std::vector<HEnumInfo> Infos;
        Infos.reserve(HEnums.size());
        
        for (const auto& Pair : HEnums)
        {
            Infos.push_back(Pair.second);
        }
        
        return Infos;
    }

    // 代码生成方法
    std::string HelianthusReflectionSystem::GenerateHClassCode(const std::string& ClassName) const
    {
        const HClassInfo* ClassInfo = GetHClassInfo(ClassName);
        if (!ClassInfo) return "";
        
        std::string Code = "class " + ClassName + " : public HObject\n{\npublic:\n";
        
        // 生成属性
        for (const auto& Prop : ClassInfo->Properties)
        {
            Code += "    " + Prop.PropertyType + " " + Prop.PropertyName + ";\n";
        }
        
        Code += "};\n";
        return Code;
    }

    std::string HelianthusReflectionSystem::GenerateHPropertyCode(const std::string& ClassName, const std::string& PropertyName) const
    {
        const HPropertyInfo* PropertyInfo = GetHPropertyInfo(ClassName, PropertyName);
        if (!PropertyInfo) return "";
        
        return PropertyInfo->PropertyType + " " + PropertyName + ";";
    }

    std::string HelianthusReflectionSystem::GenerateHFunctionCode(const std::string& ClassName, const std::string& FunctionName) const
    {
        const HFunctionInfo* FunctionInfo = GetHFunctionInfo(ClassName, FunctionName);
        if (!FunctionInfo) return "";
        
        std::string Code = FunctionInfo->ReturnType + " " + FunctionName + "(";
        
        for (size_t i = 0; i < FunctionInfo->Parameters.size(); ++i)
        {
            if (i > 0) Code += ", ";
            Code += FunctionInfo->Parameters[i].ParameterType + " " + FunctionInfo->Parameters[i].ParameterName;
        }
        
        Code += ");";
        return Code;
    }

    // 脚本绑定方法
    std::string HelianthusReflectionSystem::GenerateScriptBindings(const std::string& Language) const
    {
        std::string Bindings = "-- Helianthus 反射系统脚本绑定 (" + Language + ")\n\n";
        
        for (const auto& ClassPair : HClasses)
        {
            const HClassInfo& ClassInfo = ClassPair.second;
            Bindings += "-- 类: " + ClassInfo.ClassName + "\n";
            
            for (const auto& Prop : ClassInfo.Properties)
            {
                Bindings += "--   属性: " + Prop.PropertyName + " (" + Prop.PropertyType + ")\n";
            }
            
            for (const auto& Func : ClassInfo.Functions)
            {
                Bindings += "--   方法: " + Func.FunctionName + "() -> " + Func.ReturnType + "\n";
            }
            
            Bindings += "\n";
        }
        
        return Bindings;
    }

    bool HelianthusReflectionSystem::SaveScriptBindings(const std::string& FilePath, const std::string& Language) const
    {
        std::string Bindings = GenerateScriptBindings(Language);
        
        // 简化实现，实际应该写入文件
        std::cout << "保存脚本绑定到: " << FilePath << std::endl;
        std::cout << "绑定内容:\n" << Bindings << std::endl;
        
        return true;
    }

    // 初始化函数
    void InitializeHelianthusReflectionSystem()
    {
        if (!GHelianthusReflectionSystem)
        {
            GHelianthusReflectionSystem = &HelianthusReflectionSystem::GetInstance();
            std::cout << "Helianthus 反射系统已初始化" << std::endl;
        }
    }

    void ShutdownHelianthusReflectionSystem()
    {
        if (GHelianthusReflectionSystem)
        {
            GHelianthusReflectionSystem = nullptr;
            std::cout << "Helianthus 反射系统已关闭" << std::endl;
        }
    }

} // namespace Helianthus::Reflection
