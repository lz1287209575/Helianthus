#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#include "Reflection.h"

namespace Helianthus::Reflection
{
// 创建类信息的辅助函数
inline HClassInfo CreateClassInfo(const std::string& ClassName, const std::string& BaseClassName)
{
    HClassInfo Info;
    Info.ClassName = ClassName;
    Info.BaseClassName = BaseClassName;
    Info.ClassFlags = HClassFlags::HELIANTHUS_CLASS;
    return Info;
}

// 自动注册辅助函数
inline void RegisterClass(const std::string& ClassName, const std::string& BaseClassName)
{
    std::cout << "尝试注册类: " << ClassName << std::endl;
    if (GHelianthusReflectionSystem)
    {
        HClassInfo ClassInfo = CreateClassInfo(ClassName, BaseClassName);
        GHelianthusReflectionSystem->RegisterHClass(ClassInfo);
    }
    else
    {
        std::cout << "GHelianthusReflectionSystem 为 nullptr" << std::endl;
    }
}

inline void RegisterProperty(const std::string& ClassName,
                             const std::string& PropertyName,
                             const std::string& PropertyType)
{
    if (GHelianthusReflectionSystem)
    {
        HPropertyInfo PropInfo;
        PropInfo.PropertyName = PropertyName;
        PropInfo.PropertyType = PropertyType;
        PropInfo.PropertyFlags = HPropertyFlags::HELIANTHUS_PROPERTY;
        GHelianthusReflectionSystem->RegisterHProperty(ClassName, PropInfo);
    }
}

inline void RegisterFunction(const std::string& ClassName,
                             const std::string& FunctionName,
                             const std::string& ReturnType)
{
    if (GHelianthusReflectionSystem)
    {
        HFunctionInfo FuncInfo;
        FuncInfo.FunctionName = FunctionName;
        FuncInfo.ReturnType = ReturnType;
        FuncInfo.FunctionFlags = HFunctionFlags::HELIANTHUS_FUNCTION;
        GHelianthusReflectionSystem->RegisterHFunction(ClassName, FuncInfo);
    }
}

inline void RegisterEnum(const std::string& EnumName)
{
    if (GHelianthusReflectionSystem)
    {
        HEnumInfo EnumInfo;
        EnumInfo.EnumName = EnumName;
        GHelianthusReflectionSystem->RegisterHEnum(EnumInfo);
    }
}

}  // namespace Helianthus::Reflection

// 简化的宏定义
#define HCLASS(ClassName, BaseClass) class ClassName : public BaseClass

#define HPROPERTY(PropertyName, PropertyType) PropertyType PropertyName;

#define HFUNCTION(FunctionName, ReturnType, ...) ReturnType FunctionName(__VA_ARGS__)

#define HENUM(EnumName) enum class EnumName : int32_t

// 自动注册宏 - 使用命名空间避免语法错误
#define REGISTER_CLASS(ClassName)                                                                  \
    namespace AutoRegister##ClassName                                                              \
    {                                                                                              \
        static bool Registered =                                                                   \
            (Helianthus::Reflection::RegisterClass(#ClassName, #ClassName), true);                 \
    }

#define REGISTER_PROPERTY(ClassName, PropertyName, PropertyType)                                   \
    namespace AutoRegister##ClassName##_##PropertyName                                             \
    {                                                                                              \
        static bool Registered =                                                                   \
            (Helianthus::Reflection::RegisterProperty(#ClassName, #PropertyName, #PropertyType),   \
             true);                                                                                \
    }

#define REGISTER_FUNCTION(ClassName, FunctionName, ReturnType, ...)                                \
    namespace AutoRegister##ClassName##_##FunctionName                                             \
    {                                                                                              \
        static bool Registered =                                                                   \
            (Helianthus::Reflection::RegisterFunction(#ClassName, #FunctionName, #ReturnType),     \
             true);                                                                                \
    }

#define REGISTER_ENUM(EnumName)                                                                    \
    namespace AutoRegister##EnumName                                                               \
    {                                                                                              \
        static bool Registered = (Helianthus::Reflection::RegisterEnum(#EnumName), true);          \
    }
