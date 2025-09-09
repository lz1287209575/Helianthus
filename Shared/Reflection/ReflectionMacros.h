#pragma once

#include <type_traits>
#include <vector>
#include <string>
#include "ReflectionCore.h"

#define HCLASS(...)                                                                                   \
public:                                                                                               \
    static void __H_AddClassTag(const char* Tag)                                                      \
    {                                                                                                  \
        Helianthus::Reflection::ClassRegistry::Get().AddClassTag(__H_ClassName(), Tag);               \
    }                                                                                                  \
    static const char* __H_ClassName() { return __H_CLASS_NAME; }                                      \
private:

#define GENERATED_BODY()                                                                              \
public:                                                                                               \
    static const char* __H_CLASS_NAME;                                                                \
private:

#define HFUNCTION() /* marker for future codegen; runtime registration helper below */

#define HMETHOD(TagLiteral) /* marker for codegen; helper macro below */

#define HPROPERTY(TagLiteral) /* marker; register via helper macro below */

// 标记该类应注册 RPC 服务工厂（仅作为代码生成的探针）
#define HRPC_FACTORY() /* marker for codegen */

#define H_REGISTER_PROPERTY(ClassType, Member, TagLiteral)                                             \
    Helianthus::Reflection::ClassRegistry::Get().RegisterProperty(                                     \
        #ClassType, #Member, #TagLiteral,                                                              \
        static_cast<std::ptrdiff_t>(reinterpret_cast<char*>(&(reinterpret_cast<ClassType*>(0)->Member)) - reinterpret_cast<char*>(reinterpret_cast<ClassType*>(0))), \
        sizeof(((ClassType*)0)->Member))

// Helpers for auto register methods under class scope
#define H_REGISTER_METHOD(ClassType, Method, TagLiteral)                                               \
    Helianthus::Reflection::ClassRegistry::Get().RegisterMethod(#ClassType, #Method, #TagLiteral)

// Auto-register (static-init) convenience macros usable inside class
#define HMETHOD_AUTO(ClassType, Method, TagLiteral)                                                    \
    inline static int __h_method_reg_##Method = [](){                                                  \
        Helianthus::Reflection::ClassRegistry::Get().RegisterMethod(#ClassType, #Method, #TagLiteral); \
        return 0;                                                                                      \
    }();

#define HPROPERTY_AUTO(ClassType, Member, TagLiteral)                                                  \
    inline static int __h_prop_reg_##Member = [](){                                                    \
        H_REGISTER_PROPERTY(ClassType, Member, TagLiteral);                                            \
        return 0;                                                                                      \
    }();



