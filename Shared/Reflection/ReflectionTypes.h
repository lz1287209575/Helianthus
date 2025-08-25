#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Helianthus::Reflection
{
    // 反射类型枚举
    enum class ReflectionType
    {
        Void,
        Bool,
        Int8,
        Int16,
        Int32,
        Int64,
        UInt8,
        UInt16,
        UInt32,
        UInt64,
        Float,
        Double,
        String,
        Object,
        Array,
        Function,
        Enum,
        Struct,
        Class
    };

    // 反射属性信息
    struct PropertyInfo
    {
        std::string Name;
        std::string TypeName;
        ReflectionType Type;
        bool IsReadOnly = false;
        bool IsStatic = false;
        std::string Description;
        
        // 属性访问器
        std::function<void*(void*)> Getter;
        std::function<void(void*, void*)> Setter;
    };

    // 反射方法参数信息
    struct ParameterInfo
    {
        std::string Name;
        std::string TypeName;
        ReflectionType Type;
        bool IsOptional = false;
        std::string DefaultValue;
        std::string Description;
    };

    // 反射方法信息
    struct MethodInfo
    {
        std::string Name;
        std::string ReturnTypeName;
        ReflectionType ReturnType;
        std::vector<ParameterInfo> Parameters;
        bool IsStatic = false;
        bool IsVirtual = false;
        bool IsConst = false;
        std::string Description;
        
        // 方法调用器
        std::function<void*(void*, const std::vector<void*>&)> Invoker;
    };

    // 反射枚举值信息
    struct EnumValueInfo
    {
        std::string Name;
        int64_t Value;
        std::string Description;
    };

    // 反射枚举信息
    struct EnumInfo
    {
        std::string Name;
        std::string TypeName;
        std::vector<EnumValueInfo> Values;
        std::string Description;
    };

    // 反射类信息
    struct ClassInfo
    {
        ClassInfo() : TypeIndex(typeid(void)) {}
        
        std::string Name;
        std::string FullName;
        std::string BaseClassName;
        std::type_index TypeIndex;
        
        std::vector<PropertyInfo> Properties;
        std::vector<MethodInfo> Methods;
        std::vector<std::string> BaseClasses;
        
        bool IsAbstract = false;
        bool IsFinal = false;
        std::string Description;
        
        // 对象生命周期管理
        std::function<void*(void*)> Constructor;
        std::function<void(void*)> Destructor;
        std::function<void*(void*)> CopyConstructor;
    };

    // 反射系统接口
    class IReflectionSystem
    {
    public:
        virtual ~IReflectionSystem() = default;

        // 类型注册
        virtual void RegisterClass(const ClassInfo& ClassInfo) = 0;
        virtual void RegisterEnum(const EnumInfo& EnumInfo) = 0;
        
        // 类型查询
        virtual const ClassInfo* GetClassInfo(const std::string& ClassName) const = 0;
        virtual const ClassInfo* GetClassInfo(const std::type_index& TypeIndex) const = 0;
        virtual const EnumInfo* GetEnumInfo(const std::string& EnumName) const = 0;
        
        // 类型枚举
        virtual std::vector<std::string> GetAllClassNames() const = 0;
        virtual std::vector<std::string> GetAllEnumNames() const = 0;
        
        // 对象创建
        virtual void* CreateObject(const std::string& ClassName) = 0;
        virtual void DestroyObject(const std::string& ClassName, void* Object) = 0;
        
        // 属性访问
        virtual void* GetProperty(void* Object, const std::string& PropertyName) = 0;
        virtual void SetProperty(void* Object, const std::string& PropertyName, void* Value) = 0;
        
        // 方法调用
        virtual void* CallMethod(void* Object, const std::string& MethodName, 
                                const std::vector<void*>& Arguments) = 0;
        
        // 类型检查
        virtual bool IsInstanceOf(void* Object, const std::string& ClassName) const = 0;
        virtual bool IsSubclassOf(const std::string& ClassName, const std::string& BaseClassName) const = 0;
    };

    // 反射系统实现
    class ReflectionSystem : public IReflectionSystem
    {
    public:
        void RegisterClass(const ClassInfo& ClassInfo) override;
        void RegisterEnum(const EnumInfo& EnumInfo) override;
        
        const ClassInfo* GetClassInfo(const std::string& ClassName) const override;
        const ClassInfo* GetClassInfo(const std::type_index& TypeIndex) const override;
        const EnumInfo* GetEnumInfo(const std::string& EnumName) const override;
        
        std::vector<std::string> GetAllClassNames() const override;
        std::vector<std::string> GetAllEnumNames() const override;
        
        void* CreateObject(const std::string& ClassName) override;
        void DestroyObject(const std::string& ClassName, void* Object) override;
        
        void* GetProperty(void* Object, const std::string& PropertyName) override;
        void SetProperty(void* Object, const std::string& PropertyName, void* Value) override;
        
        void* CallMethod(void* Object, const std::string& MethodName, 
                        const std::vector<void*>& Arguments) override;
        
        bool IsInstanceOf(void* Object, const std::string& ClassName) const override;
        bool IsSubclassOf(const std::string& ClassName, const std::string& BaseClassName) const override;

    private:
        std::unordered_map<std::string, ClassInfo> Classes;
        std::unordered_map<std::type_index, ClassInfo*> TypeToClass;
        std::unordered_map<std::string, EnumInfo> Enums;
    };

    // 全局反射系统实例
    extern std::unique_ptr<ReflectionSystem> GlobalReflectionSystem;

    // 反射系统初始化
    void InitializeReflectionSystem();
    void ShutdownReflectionSystem();

} // namespace Helianthus::Reflection
