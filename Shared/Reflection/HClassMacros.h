#pragma once

#include "HClassReflection.h"
#include <string>
#include <functional>

// Core macros for clean reflection
#define HCLASS() \
    public: \
        static Helianthus::Reflection::HClass* StaticClass(); \
        virtual Helianthus::Reflection::HClass* GetClass() const override { return StaticClass(); } \
        virtual const std::string& GetClassName() const override { static std::string Name = #ClassName; return Name; } \
    private: \
        static Helianthus::Reflection::HClass* ClassInfo; \
        static bool bClassRegistered

#define HPROPERTY(PropertyName, Type) \
    struct Property_##PropertyName \
    { \
        static Helianthus::Reflection::HProperty PropertyInfo; \
        static void Register(Helianthus::Reflection::HClass* Class) \
        { \
            PropertyInfo.Name = #PropertyName; \
            PropertyInfo.TypeName = #Type; \
            PropertyInfo.Type = Helianthus::Reflection::GetPropertyType<Type>(); \
            PropertyInfo.Getter = [](Helianthus::Reflection::HObject* Obj) -> void* { \
                return &static_cast<ClassName*>(Obj)->PropertyName; \
            }; \
            PropertyInfo.Setter = [](Helianthus::Reflection::HObject* Obj, void* Value) { \
                static_cast<ClassName*>(Obj)->PropertyName = *static_cast<Type*>(Value); \
            }; \
            Class->Properties.push_back(PropertyInfo); \
        } \
    }; \
    Type PropertyName

#define HFUNCTION(FunctionName, ReturnType, ...) \
    struct Function_##FunctionName \
    { \
        static Helianthus::Reflection::HFunction FunctionInfo; \
        static void Register(Helianthus::Reflection::HClass* Class) \
        { \
            FunctionInfo.Name = #FunctionName; \
            FunctionInfo.ReturnTypeName = #ReturnType; \
            FunctionInfo.ReturnType = Helianthus::Reflection::GetPropertyType<ReturnType>(); \
            FunctionInfo.Invoker = [](Helianthus::Reflection::HObject* Obj, const std::vector<void*>& Args) -> void* { \
                static ReturnType Result; \
                Result = static_cast<ClassName*>(Obj)->FunctionName(__VA_ARGS__); \
                return &Result; \
            }; \
            Class->Functions.push_back(FunctionInfo); \
        } \
    }; \
    ReturnType FunctionName(__VA_ARGS__)

#define HCLASS_BODY(ClassName, SuperClassName) \
    Helianthus::Reflection::HClass* ClassName::ClassInfo = nullptr; \
    bool ClassName::bClassRegistered = false; \
    Helianthus::Reflection::HClass* ClassName::StaticClass() \
    { \
        if (!bClassRegistered) \
        { \
            ClassInfo = new Helianthus::Reflection::HClass(); \
            ClassInfo->Name = #ClassName; \
            ClassInfo->SuperClass = SuperClassName::StaticClass(); \
            ClassInfo->ClassSize = sizeof(ClassName); \
            ClassInfo->Constructor = []() -> Helianthus::Reflection::HObject* { return new ClassName(); }; \
            ClassInfo->Destructor = [](Helianthus::Reflection::HObject* Obj) { delete static_cast<ClassName*>(Obj); }; \
            RegisterProperties(ClassInfo); \
            RegisterFunctions(ClassInfo); \
            Helianthus::Reflection::HReflectionRegistry::Get().RegisterClass(ClassInfo); \
            bClassRegistered = true; \
        } \
        return ClassInfo; \
    }

// Property type mapping
namespace Helianthus::Reflection
{
    template<typename T>
    EPropertyType GetPropertyType();
    
    template<> EPropertyType GetPropertyType<int8_t>() { return EPropertyType::Int8; }
    template<> EPropertyType GetPropertyType<int16_t>() { return EPropertyType::Int16; }
    template<> EPropertyType GetPropertyType<int32_t>() { return EPropertyType::Int32; }
    template<> EPropertyType GetPropertyType<int64_t>() { return EPropertyType::Int64; }
    template<> EPropertyType GetPropertyType<uint8_t>() { return EPropertyType::UInt8; }
    template<> EPropertyType GetPropertyType<uint16_t>() { return EPropertyType::UInt16; }
    template<> EPropertyType GetPropertyType<uint32_t>() { return EPropertyType::UInt32; }
    template<> EPropertyType GetPropertyType<uint64_t>() { return EPropertyType::UInt64; }
    template<> EPropertyType GetPropertyType<float>() { return EPropertyType::Float; }
    template<> EPropertyType GetPropertyType<double>() { return EPropertyType::Double; }
    template<> EPropertyType GetPropertyType<bool>() { return EPropertyType::Bool; }
    template<> EPropertyType GetPropertyType<std::string>() { return EPropertyType::String; }
    template<> EPropertyType GetPropertyType<std::string>() { return EPropertyType::String; }
}

// Usage macros
#define HCLASS_BEGIN(ClassName) \
    class ClassName : public Helianthus::Reflection::HObject \
    { \
    public: \
        HCLASS() \

#define HCLASS_END(ClassName, SuperClassName) \
    private: \
        static void RegisterProperties(Helianthus::Reflection::HClass* Class); \
        static void RegisterFunctions(Helianthus::Reflection::HClass* Class); \
    }; \
    HCLASS_BODY(ClassName, SuperClassName)

// Quick property registration
#define HREGISTER_PROPERTY(Class, PropertyName, Type) \
    static Helianthus::Reflection::HProperty Property_##PropertyName; \
    Property_##PropertyName.Name = #PropertyName; \
    Property_##PropertyName.TypeName = #Type; \
    Property_##PropertyName.Type = Helianthus::Reflection::GetPropertyType<Type>(); \
    Property_##PropertyName.Getter = [](Helianthus::Reflection::HObject* Obj) -> void* { \
        return &static_cast<Class*>(Obj)->PropertyName; \
    }; \
    Property_##PropertyName.Setter = [](Helianthus::Reflection::HObject* Obj, void* Value) { \
        static_cast<Class*>(Obj)->PropertyName = *static_cast<Type*>(Value); \
    }; \
    Class->Properties.push_back(Property_##PropertyName)

// Quick function registration
#define HREGISTER_FUNCTION(Class, FunctionName, ReturnType, ...) \
    static Helianthus::Reflection::HFunction Function_##FunctionName; \
    Function_##FunctionName.Name = #FunctionName; \
    Function_##FunctionName.ReturnTypeName = #ReturnType; \
    Function_##FunctionName.ReturnType = Helianthus::Reflection::GetPropertyType<ReturnType>(); \
    Function_##FunctionName.Invoker = [](Helianthus::Reflection::HObject* Obj, const std::vector<void*>& Args) -> void* { \
        static ReturnType Result; \
        Result = static_cast<Class*>(Obj)->FunctionName(__VA_ARGS__); \
        return &Result; \
    }; \
    Class->Functions.push_back(Function_##FunctionName)