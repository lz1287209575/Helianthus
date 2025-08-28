#pragma once

#include "AttributeReflection.h"
#include <string>

// 属性宏定义
#define HPROPERTY() \
    __attribute__((annotate("property")))

#define HFUNCTION() \
    __attribute__((annotate("function")))

#define HCLASS() \
    __attribute__((annotate("class")))

#define HENUM() \
    __attribute__((annotate("enum")))

// 属性描述宏
#define HDESCRIPTION(Desc) \
    __attribute__((annotate("description:" #Desc)))

#define HREADONLY() \
    __attribute__((annotate("readonly")))

#define HBLUEPRINTREADWRITE() \
    __attribute__((annotate("blueprintreadwrite")))

// 简化使用方式
#define HREFLECT() \
    __attribute__((annotate("reflect")))

// 编译期类型信息
namespace Helianthus::Reflection::Attribute
{
    // 编译期类型信息存储
    template<typename T>
    struct TypeInfo
    {
        static constexpr const char* Name = typeid(T).name();
        static constexpr size_t Size = sizeof(T);
    };
    
    // 属性信息存储
    template<typename Class, typename PropertyType>
    struct PropertyInfo
    {
        static constexpr const char* ClassName = typeid(Class).name();
        static constexpr const char* PropertyName = "";
        static constexpr size_t Offset = 0;
        static constexpr EAttributeType Type = GetAttributeType<PropertyType>();
    };
    
    // 函数信息存储
    template<typename Class, typename ReturnType, typename... Args>
    struct FunctionInfo
    {
        static constexpr const char* ClassName = typeid(Class).name();
        static constexpr const char* FunctionName = "";
        static constexpr EAttributeType ReturnTypeInfo = GetAttributeType<ReturnType>();
    };
}

// 运行时属性访问
#define GET_PROPERTY(ClassType, PropertyName) \
    Helianthus::Reflection::Attribute::ReflectionRegistry::Get().GetProperty(typeid(ClassType).name(), #PropertyName)

#define GET_FUNCTION(ClassType, FunctionName) \
    Helianthus::Reflection::Attribute::ReflectionRegistry::Get().GetFunction(typeid(ClassType).name(), #FunctionName)

#define CREATE_OBJECT(ClassType) \
    Helianthus::Reflection::Attribute::ReflectionRegistry::Get().CreateObject(typeid(ClassType).name())