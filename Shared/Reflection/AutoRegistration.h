#pragma once

#include "ReflectionTypes.h"
#include <type_traits>
#include <unordered_map>
#include <functional>
#include <memory>
#include <iostream>

namespace Helianthus::Reflection
{
    // 自动注册器基类
    class IAutoRegistrar
    {
    public:
        virtual ~IAutoRegistrar() = default;
        virtual void Register(ReflectionSystem* System) = 0;
        virtual std::string GetTypeName() const = 0;
    };

    // 自动注册管理器
    class AutoRegistrationManager
    {
    public:
        static AutoRegistrationManager& GetInstance()
        {
            static AutoRegistrationManager Instance;
            return Instance;
        }

        // 注册自动注册器
        void RegisterRegistrar(std::unique_ptr<IAutoRegistrar> Registrar)
        {
            if (Registrar)
            {
                Registrars.push_back(std::move(Registrar));
            }
        }

        // 执行所有注册
        void PerformAllRegistrations(ReflectionSystem* System)
        {
            for (auto& Registrar : Registrars)
            {
                try
                {
                    Registrar->Register(System);
                }
                catch (const std::exception& e)
                {
                    // 记录错误但继续执行
                    std::cerr << "Auto registration failed for " 
                              << Registrar->GetTypeName() 
                              << ": " << e.what() << std::endl;
                }
            }
        }

        // 清空注册器列表
        void Clear()
        {
            Registrars.clear();
        }

    private:
        std::vector<std::unique_ptr<IAutoRegistrar>> Registrars;
    };

    // 模板自动注册器
    template<typename T>
    class AutoRegistrar : public IAutoRegistrar
    {
    public:
        AutoRegistrar()
        {
            // 在构造时自动注册到管理器
            AutoRegistrationManager::GetInstance().RegisterRegistrar(
                std::unique_ptr<IAutoRegistrar>(this)
            );
        }

        std::string GetTypeName() const override
        {
            return typeid(T).name();
        }

        void Register(ReflectionSystem* System) override
        {
            if constexpr (HasReflectionInfo<T>::value)
            {
                if (System)
                {
                    System->RegisterClass(T::GetReflectionInfo());
                }
            }
        }

    private:
        // 检查类型是否有反射信息
        template<typename U>
        struct HasReflectionInfo
        {
        private:
            template<typename V>
            static auto Test(void*) -> decltype(V::GetReflectionInfo(), std::true_type{});
            template<typename>
            static std::false_type Test(...);
        public:
            static constexpr bool value = decltype(Test<U>(nullptr))::value;
        };
    };

    // 智能自动注册宏
    #define HELIANTHUS_SMART_REGISTER(ClassName) \
        static Helianthus::Reflection::AutoRegistrar<ClassName> ClassName##AutoRegistrar

    // 属性自动检测宏
    #define HELIANTHUS_AUTO_PROPERTY(PropertyName) \
        HELIANTHUS_REFLECT_PROPERTY(PropertyName, decltype(PropertyName), \
            [](auto* obj) -> decltype(auto) { return obj->PropertyName; }, \
            [](auto* obj, auto value) { obj->PropertyName = value; })

    // 方法自动检测宏
    #define HELIANTHUS_AUTO_METHOD(MethodName) \
        HELIANTHUS_REFLECT_METHOD(MethodName, decltype(std::declval<decltype(this)>()->MethodName()), \
            decltype(std::declval<decltype(this)>()->MethodName()))

    // 构造函数自动检测
    template<typename T>
    struct ConstructorDetector
    {
        template<typename... Args>
        static auto HasConstructor(void*) -> decltype(T(Args{}...), std::true_type{});
        template<typename...>
        static std::false_type HasConstructor(...);

        template<typename... Args>
        static constexpr bool HasDefaultConstructor = decltype(HasConstructor<Args...>(nullptr))::value;
    };

    // 属性自动检测
    template<typename T>
    struct PropertyDetector
    {
        template<typename U>
        static auto DetectProperties(U*) -> decltype(U::GetReflectionInfo(), std::true_type{});
        template<typename>
        static std::false_type DetectProperties(...);

        static constexpr bool HasProperties = decltype(DetectProperties<T>(nullptr))::value;
    };

    // 自动属性注册器
    template<typename T>
    class AutoPropertyRegistrar
    {
    public:
        static void RegisterProperties(ClassInfo& Info)
        {
            // 使用SFINAE检测公共成员变量
            RegisterPublicMembers<T>(Info);
        }

    private:
        template<typename U>
        static void RegisterPublicMembers(ClassInfo& Info)
        {
            // 这里需要更复杂的实现来检测公共成员
            // 简化版本，实际需要编译器特定的实现
        }
    };

    // 智能类注册宏
    #define HELIANTHUS_SMART_CLASS(ClassName, BaseClassName) \
        HELIANTHUS_REFLECT_CLASS(ClassName, BaseClassName) { \
            /* 自动检测并注册属性 */ \
            Helianthus::Reflection::AutoPropertyRegistrar<ClassName>::RegisterProperties(Info); \
            /* 自动检测并注册方法 */ \
            Helianthus::Reflection::AutoMethodRegistrar<ClassName>::RegisterMethods(Info); \
        } \
        HELIANTHUS_SMART_REGISTER(ClassName)

    // 自动方法注册器
    template<typename T>
    class AutoMethodRegistrar
    {
    public:
        static void RegisterMethods(ClassInfo& Info)
        {
            // 使用SFINAE检测公共方法
            RegisterPublicMethods<T>(Info);
        }

    private:
        template<typename U>
        static void RegisterPublicMethods(ClassInfo& Info)
        {
            // 这里需要更复杂的实现来检测公共方法
            // 简化版本，实际需要编译器特定的实现
        }
    };

    // 编译时类型信息收集器
    template<typename T>
    struct TypeInfoCollector
    {
        static constexpr auto TypeName = typeid(T).name();
        static constexpr auto Size = sizeof(T);
        static constexpr auto Alignment = alignof(T);
        
        // 检测是否有虚函数
        static constexpr bool HasVirtualFunctions = std::is_polymorphic_v<T>;
        
        // 检测是否可默认构造
        static constexpr bool IsDefaultConstructible = std::is_default_constructible_v<T>;
        
        // 检测是否可复制
        static constexpr bool IsCopyable = std::is_copy_constructible_v<T>;
        
        // 检测是否可移动
        static constexpr bool IsMovable = std::is_move_constructible_v<T>;
    };

    // 自动注册初始化器
    class AutoRegistrationInitializer
    {
    public:
        static void Initialize(ReflectionSystem* System)
        {
            if (System)
            {
                AutoRegistrationManager::GetInstance().PerformAllRegistrations(System);
            }
        }

        static void Shutdown()
        {
            AutoRegistrationManager::GetInstance().Clear();
        }
    };

} // namespace Helianthus::Reflection
