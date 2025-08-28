#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Helianthus::Reflection
{
    // 基础对象类
    class HObject
    {
    public:
        HObject() = default;
        virtual ~HObject() = default;
        
        virtual const std::string& GetClassName() const;
        virtual const std::string& GetName() const { return ObjectName; }
        virtual void SetName(const std::string& Name) { ObjectName = Name; }
        
        template<typename T>
        bool IsA() const;
        
        template<typename T>
        T* Cast();
        
        template<typename T>
        const T* Cast() const;
        
    protected:
        std::string ObjectName = "HObject";
    };
    
    // 反射接口
    class IReflection
    {
    public:
        virtual ~IReflection() = default;
        virtual const std::string& GetClassName() const = 0;
    };
    
    // 属性访问器
    template<typename T>
    class PropertyAccessor
    {
    public:
        PropertyAccessor(void* Obj, size_t Offset)
            : Object(Obj), MemberOffset(Offset) {}
        
        T Get() const
        {
            return *reinterpret_cast<T*>(static_cast<char*>(Object) + MemberOffset);
        }
        
        void Set(const T& Value)
        {
            *reinterpret_cast<T*>(static_cast<char*>(Object) + MemberOffset) = Value;
        }
        
    private:
        void* Object;
        size_t MemberOffset;
    };
    
    // 函数调用器
    template<typename ReturnType, typename... Args>
    class FunctionCaller
    {
    public:
        using FunctionPtr = ReturnType (HObject::*)(Args...);
        
        FunctionCaller(HObject* Obj, FunctionPtr Func)
            : Object(Obj), Function(Func) {}
        
        ReturnType Call(Args... args)
        {
            return (Object->*Function)(args...);
        }
        
    private:
        HObject* Object;
        FunctionPtr Function;
    };
}