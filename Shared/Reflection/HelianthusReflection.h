#pragma once

#include "ReflectionTypes.h"
#include <type_traits>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Helianthus::Reflection
{
    
    // Helianthus风格的类信息
    struct HelianthusClassInfo : public ClassInfo
    {
        HelianthusClassInfo* SuperClass = nullptr;
        std::vector<HelianthusClassInfo*> SubClasses;
        
        // 检查继承关系
        bool IsChildOf(const HelianthusClassInfo* Other) const
        {
            if (!Other) return false;
            if (this == Other) return true;
            if (SuperClass) return SuperClass->IsChildOf(Other);
            return false;
        }
        
        // 获取所有属性（包括继承的）
        std::vector<PropertyInfo> GetAllProperties() const
        {
            std::vector<PropertyInfo> AllProperties;
            if (SuperClass)
            {
                AllProperties = SuperClass->GetAllProperties();
            }
            AllProperties.insert(AllProperties.end(), Properties.begin(), Properties.end());
            return AllProperties;
        }
        
        // 获取所有方法（包括继承的）
        std::vector<MethodInfo> GetAllMethods() const
        {
            std::vector<MethodInfo> AllMethods;
            if (SuperClass)
            {
                AllMethods = SuperClass->GetAllMethods();
            }
            AllMethods.insert(AllMethods.end(), Methods.begin(), Methods.end());
            return AllMethods;
        }
    };

    // Helianthus风格的反射基类
    class HelianthusObject
    {
    public:
        virtual ~HelianthusObject() = default;
        
        // 获取类信息
        virtual const HelianthusClassInfo* GetClass() const = 0;
        
        // 获取类名
        virtual const std::string& GetClassName() const = 0;
        
        // 是否是某个类的实例
        template<typename T>
        bool IsA() const
        {
            return GetClass()->IsChildOf(T::StaticClass());
        }
        
        // 动态转换
        template<typename T>
        T* Cast()
        {
            if (IsA<T>())
            {
                return static_cast<T*>(this);
            }
            return nullptr;
        }
        
        template<typename T>
        const T* Cast() const
        {
            if (IsA<T>())
            {
                return static_cast<const T*>(this);
            }
            return nullptr;
        }
    };

    // Helianthus风格的反射系统
    class HelianthusReflectionSystem
    {
    public:
        static HelianthusReflectionSystem& Get()
        {
            static HelianthusReflectionSystem Instance;
            return Instance;
        }
        
        // 注册类
        void RegisterClass(HelianthusClassInfo* ClassInfo)
        {
            Classes[ClassInfo->Name] = ClassInfo;
            TypeToClass[ClassInfo->TypeIndex] = ClassInfo;
            
            // 建立继承关系
            if (ClassInfo->SuperClass)
            {
                ClassInfo->SuperClass->SubClasses.push_back(ClassInfo);
            }
        }
        
        // 获取类信息
        HelianthusClassInfo* GetClass(const std::string& ClassName)
        {
            auto It = Classes.find(ClassName);
            return (It != Classes.end()) ? It->second : nullptr;
        }
        
        HelianthusClassInfo* GetClass(const std::type_index& TypeIndex)
        {
            auto It = TypeToClass.find(TypeIndex);
            return (It != TypeToClass.end()) ? It->second : nullptr;
        }
        
        // 创建对象
        template<typename T>
        T* CreateObject()
        {
            HelianthusClassInfo* ClassInfo = GetClass(typeid(T));
            if (ClassInfo && ClassInfo->Constructor)
            {
                return static_cast<T*>(ClassInfo->Constructor(nullptr));
            }
            return nullptr;
        }
        
        // 销毁对象
        void DestroyObject(HelianthusObject* Object)
        {
            if (Object)
            {
                const HelianthusClassInfo* ClassInfo = Object->GetClass();
                if (ClassInfo && ClassInfo->Destructor)
                {
                    ClassInfo->Destructor(Object);
                }
            }
        }
        
    private:
        std::unordered_map<std::string, HelianthusClassInfo*> Classes;
        std::unordered_map<std::type_index, HelianthusClassInfo*> TypeToClass;
    };

    // Helianthus风格的宏系统
    #define HELIANTHUS_CLASS(ClassName, SuperClassName) \
        class ClassName : public SuperClassName \
        { \
        public: \
            static HelianthusClassInfo* StaticClass(); \
            virtual const HelianthusClassInfo* GetClass() const override { return StaticClass(); } \
            virtual const std::string& GetClassName() const override { static std::string Name = #ClassName; return Name; } \
            HELIANTHUS_GENERATED_BODY() \
        private: \
            static HelianthusClassInfo* ClassInfo; \
            static bool bClassInitialized;

    #define HELIANTHUS_GENERATED_BODY() \
        public: \
            ClassName() {} \
            virtual ~ClassName() {}

    #define HELIANTHUS_PROPERTY(PropertyName, PropertyType, ...) \
        PropertyType PropertyName; \
        static PropertyInfo PropertyName##_PropertyInfo; \
        static void Register##PropertyName##Property(HelianthusClassInfo* ClassInfo) \
        { \
            PropertyName##_PropertyInfo.Name = #PropertyName; \
            PropertyName##_PropertyInfo.TypeName = #PropertyType; \
            PropertyName##_PropertyInfo.Getter = [](void* Obj) -> void* { \
                auto* Object = static_cast<ClassName*>(Obj); \
                return static_cast<void*>(&Object->PropertyName); \
            }; \
            PropertyName##_PropertyInfo.Setter = [](void* Obj, void* Value) { \
                auto* Object = static_cast<ClassName*>(Obj); \
                Object->PropertyName = *static_cast<PropertyType*>(Value); \
            }; \
            ClassInfo->Properties.push_back(PropertyName##_PropertyInfo); \
        }

    #define HELIANTHUS_METHOD(MethodName, ReturnType, ...) \
        ReturnType MethodName(__VA_ARGS__); \
        static MethodInfo MethodName##_MethodInfo; \
        static void Register##MethodName##Method(HelianthusClassInfo* ClassInfo) \
        { \
            MethodName##_MethodInfo.Name = #MethodName; \
            MethodName##_MethodInfo.ReturnTypeName = #ReturnType; \
            MethodName##_MethodInfo.Invoker = [](void* Obj, const std::vector<void*>& Args) -> void* { \
                auto* Object = static_cast<ClassName*>(Obj); \
                return static_cast<void*>(&Object->MethodName()); \
            }; \
            ClassInfo->Methods.push_back(MethodName##_MethodInfo); \
        }

    // 自动注册宏
    #define HELIANTHUS_IMPLEMENT_CLASS(ClassName, SuperClassName) \
        HelianthusClassInfo* ClassName::ClassInfo = nullptr; \
        bool ClassName::bClassInitialized = false; \
        HelianthusClassInfo* ClassName::StaticClass() \
        { \
            if (!bClassInitialized) \
            { \
                ClassInfo = new HelianthusClassInfo(); \
                ClassInfo->Name = #ClassName; \
                ClassInfo->FullName = #ClassName; \
                ClassInfo->TypeIndex = std::type_index(typeid(ClassName)); \
                ClassInfo->SuperClass = static_cast<HelianthusClassInfo*>(SuperClassName::StaticClass()); \
                ClassInfo->IsAbstract = std::is_abstract_v<ClassName>; \
                ClassInfo->IsFinal = std::is_final_v<ClassName>; \
                \
                /* 注册属性 */ \
                RegisterProperties(ClassInfo); \
                \
                /* 注册方法 */ \
                RegisterMethods(ClassInfo); \
                \
                /* 注册到反射系统 */ \
                HelianthusReflectionSystem::Get().RegisterClass(ClassInfo); \
                \
                bClassInitialized = true; \
            } \
            return ClassInfo; \
        } \
        void ClassName::RegisterProperties(HelianthusClassInfo* ClassInfo) \
        { \
            /* 这里会由代码生成器自动填充 */ \
        } \
        void ClassName::RegisterMethods(HelianthusClassInfo* ClassInfo) \
        { \
            /* 这里会由代码生成器自动填充 */ \
        }

    // 属性访问宏
    #define HELIANTHUS_GET_PROPERTY(Object, PropertyName) \
        (Object->GetClass()->GetProperty(#PropertyName)->Getter(Object))

    #define HELIANTHUS_SET_PROPERTY(Object, PropertyName, Value) \
        do { \
            auto* Property = Object->GetClass()->GetProperty(#PropertyName); \
            if (Property && Property->Setter) \
            { \
                Property->Setter(Object, &Value); \
            } \
        } while(0)

    #define HELIANTHUS_CALL_METHOD(Object, MethodName, ...) \
        (Object->GetClass()->GetMethod(#MethodName)->Invoker(Object, {__VA_ARGS__}))

    // 类型安全的属性访问
    template<typename T, typename PropertyType>
    class HelianthusProperty
    {
    public:
        HelianthusProperty(T* Object, const std::string& PropertyName)
            : Object(Object), PropertyName(PropertyName) {}
        
        PropertyType Get() const
        {
            if (Object)
            {
                auto* Property = Object->GetClass()->GetProperty(PropertyName);
                if (Property && Property->Getter)
                {
                    return *static_cast<PropertyType*>(Property->Getter(Object));
                }
            }
            return PropertyType{};
        }
        
        void Set(const PropertyType& Value)
        {
            if (Object)
            {
                auto* Property = Object->GetClass()->GetProperty(PropertyName);
                if (Property && Property->Setter)
                {
                    Property->Setter(Object, const_cast<void*>(static_cast<const void*>(&Value)));
                }
            }
        }
        
        operator PropertyType() const { return Get(); }
        
        HelianthusProperty& operator=(const PropertyType& Value)
        {
            Set(Value);
            return *this;
        }
        
    private:
        T* Object;
        std::string PropertyName;
    };

    // 类型安全的方法调用
    template<typename T, typename ReturnType, typename... Args>
    class HelianthusMethod
    {
    public:
        HelianthusMethod(T* Object, const std::string& MethodName)
            : Object(Object), MethodName(MethodName) {}
        
        ReturnType Call(Args... args)
        {
            if (Object)
            {
                auto* Method = Object->GetClass()->GetMethod(MethodName);
                if (Method && Method->Invoker)
                {
                    std::vector<void*> Arguments = {static_cast<void*>(&args)...};
                    return *static_cast<ReturnType*>(Method->Invoker(Object, Arguments));
                }
            }
            return ReturnType{};
        }
        
        ReturnType operator()(Args... args) { return Call(args...); }
        
    private:
        T* Object;
        std::string MethodName;
    };

    // 便捷宏
    #define HELIANTHUS_PROPERTY_ACCESS(Object, PropertyName, Type) \
        HelianthusProperty<decltype(Object), Type>(Object, #PropertyName)

    #define HELIANTHUS_METHOD_ACCESS(Object, MethodName, ReturnType, ...) \
        HelianthusMethod<decltype(Object), ReturnType, __VA_ARGS__>(Object, #MethodName)

} // namespace Helianthus::Reflection
