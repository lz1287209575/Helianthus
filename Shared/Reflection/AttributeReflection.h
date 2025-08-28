#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <memory>

namespace Helianthus::Reflection::Attribute
{
    // 属性类型枚举
    enum class EAttributeType
    {
        Int8, Int16, Int32, Int64,
        UInt8, UInt16, UInt32, UInt64,
        Float, Double, Bool,
        String, Object, Struct, Enum,
        Array, Map, Set
    };
    
    // 属性属性（类似C#的Attribute）
    struct PropertyAttribute
    {
        std::string Name;
        std::string TypeName;
        EAttributeType Type;
        size_t Offset;
        size_t Size;
        bool bReadOnly = false;
        bool bBlueprintReadWrite = false;
        std::string Description;
        
        std::function<void*(void*)> Getter;
        std::function<void(void*, void*)> Setter;
    };
    
    // 函数属性
    struct FunctionAttribute
    {
        std::string Name;
        std::string ReturnTypeName;
        EAttributeType ReturnType;
        std::vector<std::string> ParameterTypes;
        bool bConst = false;
        bool bStatic = false;
        std::string Description;
        
        std::function<void*(void*, const std::vector<void*>&)> Invoker;
    };
    
    // 类信息
    struct ClassAttribute
    {
        std::string Name;
        std::string SuperClassName;
        size_t ClassSize;
        std::vector<PropertyAttribute> Properties;
        std::vector<FunctionAttribute> Functions;
        
        std::function<void*()> Constructor;
        std::function<void(void*)> Destructor;
    };
    
    // 反射注册器
    class ReflectionRegistry
    {
    public:
        static ReflectionRegistry& Get();
        
        void RegisterClass(const ClassAttribute& ClassAttr);
        const ClassAttribute* GetClass(const std::string& ClassName) const;
        const PropertyAttribute* GetProperty(const std::string& ClassName, const std::string& PropertyName) const;
        const FunctionAttribute* GetFunction(const std::string& ClassName, const std::string& FunctionName) const;
        
        std::vector<std::string> GetAllClassNames() const;
        std::vector<std::string> GetPropertyNames(const std::string& ClassName) const;
        std::vector<std::string> GetFunctionNames(const std::string& ClassName) const;
        
        void* CreateObject(const std::string& ClassName);
        void DestroyObject(void* Object, const std::string& ClassName);
        
    private:
        std::unordered_map<std::string, ClassAttribute> Classes;
    };
    
    // 全局注册器
    void InitializeAttributeReflection();
    void ShutdownAttributeReflection();
    
    // 类型映射
    template<typename T>
    EAttributeType GetAttributeType();
    
    // 辅助函数
    template<typename T>
    const ClassAttribute* GetClassAttribute();
    
} // namespace Helianthus::Reflection::Attribute