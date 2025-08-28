#pragma once

#include "AttributeReflection.h"
#include <iostream>
#include <memory>

namespace Helianthus::Reflection::Attribute
{
    // 运行时类型信息
    class RuntimeTypeInfo
    {
    public:
        static RuntimeTypeInfo& Get();
        
        template<typename ClassType>
        void RegisterClass(const std::string& ClassName);
        
        template<typename ClassType, typename PropertyType>
        void RegisterProperty(const std::string& PropertyName, PropertyType ClassType::* MemberPtr);
        
        template<typename ClassType, typename ReturnType, typename... Args>
        void RegisterFunction(const std::string& FunctionName, ReturnType (ClassType::* FuncPtr)(Args...));
        
        template<typename ClassType, typename ReturnType, typename... Args>
        void RegisterFunction(const std::string& FunctionName, ReturnType (ClassType::* FuncPtr)(Args...) const);
        
        const ClassAttribute* GetClassAttribute(const std::string& ClassName) const;
        const PropertyAttribute* GetPropertyAttribute(const std::string& ClassName, const std::string& PropertyName) const;
        const FunctionAttribute* GetFunctionAttribute(const std::string& ClassName, const std::string& FunctionName) const;
        
        template<typename ClassType>
        void* CreateObject();
        
        template<typename ClassType, typename PropertyType>
        PropertyType GetPropertyValue(void* Object, PropertyType ClassType::* MemberPtr);
        
        template<typename ClassType, typename PropertyType>
        void SetPropertyValue(void* Object, PropertyType ClassType::* MemberPtr, const PropertyType& Value);
        
    private:
        std::unordered_map<std::string, ClassAttribute> ClassRegistry;
    };
    
    // 全局运行时实例
    extern RuntimeTypeInfo GlobalRuntimeTypeInfo;
    
    // 模板实现
    template<typename ClassType>
    void RuntimeTypeInfo::RegisterClass(const std::string& ClassName)
    {
        ClassAttribute Attr;
        Attr.Name = ClassName;
        Attr.SuperClassName = typeid(HObject).name();
        Attr.ClassSize = sizeof(ClassType);
        Attr.Constructor = []() -> void* { return new ClassType(); };
        Attr.Destructor = [](void* Obj) { delete static_cast<ClassType*>(Obj); };
        
        ClassRegistry[ClassName] = std::move(Attr);
    }
    
    template<typename ClassType, typename PropertyType>
    void RuntimeTypeInfo::RegisterProperty(const std::string& PropertyName, PropertyType ClassType::* MemberPtr)
    {
        std::string ClassName = typeid(ClassType).name();
        
        PropertyAttribute PropAttr;
        PropAttr.Name = PropertyName;
        PropAttr.TypeName = typeid(PropertyType).name();
        PropAttr.Type = GetAttributeType<PropertyType>();
        PropAttr.Offset = reinterpret_cast<size_t>(&(static_cast<ClassType*>(nullptr)->*MemberPtr));
        PropAttr.Size = sizeof(PropertyType);
        
        PropAttr.Getter = [MemberPtr](void* Obj) -> void* {
            return &(static_cast<ClassType*>(Obj)->*MemberPtr);
        };
        
        PropAttr.Setter = [MemberPtr](void* Obj, void* Value) {
            static_cast<ClassType*>(Obj)->*MemberPtr = *static_cast<PropertyType*>(Value);
        };
        
        ClassRegistry[ClassName].Properties.push_back(std::move(PropAttr));
    }
    
    template<typename ClassType, typename ReturnType, typename... Args>
    void RuntimeTypeInfo::RegisterFunction(const std::string& FunctionName, ReturnType (ClassType::* FuncPtr)(Args...))
    {
        std::string ClassName = typeid(ClassType).name();
        
        FunctionAttribute FuncAttr;
        FuncAttr.Name = FunctionName;
        FuncAttr.ReturnTypeName = typeid(ReturnType).name();
        FuncAttr.ReturnType = GetAttributeType<ReturnType>();
        
        FuncAttr.Invoker = [FuncPtr](void* Obj, const std::vector<void*>& Args) -> void* {
            static ReturnType Result;
            // 这里需要处理参数解包
            // 简化实现
            return &Result;
        };
        
        ClassRegistry[ClassName].Functions.push_back(std::move(FuncAttr));
    }
    
    template<typename ClassType>
    void* RuntimeTypeInfo::CreateObject()
    {
        return new ClassType();
    }
    
    template<typename ClassType, typename PropertyType>
    PropertyType RuntimeTypeInfo::GetPropertyValue(void* Object, PropertyType ClassType::* MemberPtr)
    {
        return static_cast<ClassType*>(Object)->*MemberPtr;
    }
    
    template<typename ClassType, typename PropertyType>
    void RuntimeTypeInfo::SetPropertyValue(void* Object, PropertyType ClassType::* MemberPtr, const PropertyType& Value)
    {
        static_cast<ClassType*>(Object)->*MemberPtr = Value;
    }
}