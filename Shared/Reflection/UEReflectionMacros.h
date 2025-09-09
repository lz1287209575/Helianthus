#pragma once

#include <type_traits>
#include "UEReflection.h"

#define HCLASS(...)                                                                                   \
public:                                                                                               \
    static const char* StaticHClassName() { return __FUNCTION__; }                                     \
private:

#define GENERATED_BODY()                                                                              \
public:                                                                                               \
    struct __H_StaticRegistrar                                                                        \
    {                                                                                                \
        __H_StaticRegistrar()                                                                          \
        {                                                                                              \
            std::vector<std::string> Tags;                                                             \
            Helianthus::Reflection::ClassRegistry::Get().RegisterClass(                               \
                typeid(std::remove_pointer<decltype(this)>::type).name(),                              \
                Tags,                                                                                  \
                []() -> void* { return new std::remove_pointer<decltype(this)>::type(); });            \
        }                                                                                              \
    };                                                                                                 \
    static __H_StaticRegistrar __H_StaticRegistrarInstance;

#define HFUNCTION() /* marker for future codegen; runtime registration helper below */

#define HMETHOD(TagLiteral)                                                                            \
private:                                                                                               \
    struct __H_MethodRegistrar_##TagLiteral                                                             \
    {                                                                                                  \
        __H_MethodRegistrar_##TagLiteral(const char* ClassName, const char* MethodName)                \
        {                                                                                              \
            Helianthus::Reflection::ClassRegistry::Get().RegisterMethod(ClassName, MethodName, #TagLiteral); \
        }                                                                                              \
    };

#define HPROPERTY(TagLiteral) /* marker; register via helper macro below */

#define H_REGISTER_PROPERTY(ClassType, Member, TagLiteral)                                             \
    Helianthus::Reflection::ClassRegistry::Get().RegisterProperty(                                     \
        #ClassType, #Member, #TagLiteral,                                                              \
        static_cast<std::ptrdiff_t>(reinterpret_cast<char*>(&(reinterpret_cast<ClassType*>(0)->Member)) - reinterpret_cast<char*>(reinterpret_cast<ClassType*>(0))), \
        sizeof(((ClassType*)0)->Member))


