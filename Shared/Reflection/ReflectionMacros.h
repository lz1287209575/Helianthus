#pragma once

#include "ReflectionTypes.h"
#include <type_traits>

namespace Helianthus::Reflection
{
    // 反射注册宏
    #define HELIANTHUS_REFLECT_CLASS(ClassName, BaseClassName) \
        static const ClassInfo& GetReflectionInfo() { \
            static ClassInfo Info; \
            static bool Initialized = false; \
            if (!Initialized) { \
                Info.Name = #ClassName; \
                Info.FullName = #ClassName; \
                Info.BaseClassName = #BaseClassName; \
                Info.TypeIndex = std::type_index(typeid(ClassName)); \
                Info.IsAbstract = std::is_abstract_v<ClassName>; \
                Info.IsFinal = std::is_final_v<ClassName>; \
                RegisterClassReflection(Info); \
                Initialized = true; \
            } \
            return Info; \
        } \
        static void RegisterClassReflection(ClassInfo& Info)

    #define HELIANTHUS_REFLECT_PROPERTY(PropertyName, Type, GetterFunc, SetterFunc) \
        do { \
            PropertyInfo Prop; \
            Prop.Name = #PropertyName; \
            Prop.TypeName = #Type; \
            Prop.Type = GetReflectionType<Type>(); \
            Prop.Getter = [](void* Obj) -> void* { \
                auto* Object = static_cast<decltype(this)>(Obj); \
                return static_cast<void*>(&(Object->*GetterFunc)); \
            }; \
            Prop.Setter = [](void* Obj, void* Value) { \
                auto* Object = static_cast<decltype(this)>(Obj); \
                Object->*SetterFunc = *static_cast<Type*>(Value); \
            }; \
            Info.Properties.push_back(Prop); \
        } while(0)

    #define HELIANTHUS_REFLECT_METHOD(MethodName, ReturnType, ...) \
        do { \
            MethodInfo Method; \
            Method.Name = #MethodName; \
            Method.ReturnTypeName = #ReturnType; \
            Method.ReturnType = GetReflectionType<ReturnType>(); \
            Method.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* { \
                auto* Object = static_cast<decltype(this)>(Obj); \
                return InvokeMethod<ReturnType, __VA_ARGS__>(Object, &decltype(this)::MethodName, Args); \
            }; \
            Info.Methods.push_back(Method); \
        } while(0)

    #define HELIANTHUS_REFLECT_ENUM(EnumName) \
        static const EnumInfo& GetReflectionInfo() { \
            static EnumInfo Info; \
            static bool Initialized = false; \
            if (!Initialized) { \
                Info.Name = #EnumName; \
                Info.TypeName = #EnumName; \
                RegisterEnumReflection(Info); \
                Initialized = true; \
            } \
            return Info; \
        } \
        static void RegisterEnumReflection(EnumInfo& Info)

    #define HELIANTHUS_REFLECT_ENUM_VALUE(ValueName, Value) \
        do { \
            EnumValueInfo EnumValue; \
            EnumValue.Name = #ValueName; \
            EnumValue.Value = static_cast<int64_t>(Value); \
            Info.Values.push_back(EnumValue); \
        } while(0)

    // 辅助函数
    template<typename T>
    ReflectionType GetReflectionType()
    {
        if constexpr (std::is_same_v<T, void>) return ReflectionType::Void;
        else if constexpr (std::is_same_v<T, bool>) return ReflectionType::Bool;
        else if constexpr (std::is_same_v<T, int8_t>) return ReflectionType::Int8;
        else if constexpr (std::is_same_v<T, int16_t>) return ReflectionType::Int16;
        else if constexpr (std::is_same_v<T, int32_t>) return ReflectionType::Int32;
        else if constexpr (std::is_same_v<T, int64_t>) return ReflectionType::Int64;
        else if constexpr (std::is_same_v<T, uint8_t>) return ReflectionType::UInt8;
        else if constexpr (std::is_same_v<T, uint16_t>) return ReflectionType::UInt16;
        else if constexpr (std::is_same_v<T, uint32_t>) return ReflectionType::UInt32;
        else if constexpr (std::is_same_v<T, uint64_t>) return ReflectionType::UInt64;
        else if constexpr (std::is_same_v<T, float>) return ReflectionType::Float;
        else if constexpr (std::is_same_v<T, double>) return ReflectionType::Double;
        else if constexpr (std::is_same_v<T, std::string>) return ReflectionType::String;
        else if constexpr (std::is_enum_v<T>) return ReflectionType::Enum;
        else if constexpr (std::is_class_v<T>) return ReflectionType::Class;
        else return ReflectionType::Object;
    }

    // 方法调用辅助函数
    template<typename ReturnType, typename... Args>
    void* InvokeMethod(void* Object, ReturnType (*Method)(Args...), 
                      const std::vector<void*>& Arguments)
    {
        // 简化实现，实际需要更复杂的参数解包
        if constexpr (std::is_same_v<ReturnType, void>)
        {
            // void 返回类型
            return nullptr;
        }
        else
        {
            // 非void返回类型
            static ReturnType Result;
            Result = ReturnType{};
            return static_cast<void*>(&Result);
        }
    }

    // 自动注册宏
    #define HELIANTHUS_AUTO_REGISTER(ClassName) \
        static bool Register##ClassName() { \
            if (GlobalReflectionSystem) { \
                GlobalReflectionSystem->RegisterClass(ClassName::GetReflectionInfo()); \
                return true; \
            } \
            return false; \
        } \
        static bool ClassName##Registered = Register##ClassName()

    #define HELIANTHUS_AUTO_REGISTER_ENUM(EnumName) \
        static bool Register##EnumName() { \
            if (GlobalReflectionSystem) { \
                GlobalReflectionSystem->RegisterEnum(EnumName::GetReflectionInfo()); \
                return true; \
            } \
            return false; \
        } \
        static bool EnumName##Registered = Register##EnumName()

} // namespace Helianthus::Reflection
