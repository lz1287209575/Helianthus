#pragma once

#include "HClassMacros.h"
#include <algorithm>

namespace Helianthus::Reflection
{
    // Implementation of HClass
    bool HClass::IsChildOf(const HClass* Parent) const
    {
        if (!Parent) return false;
        if (this == Parent) return true;
        if (SuperClass) return SuperClass->IsChildOf(Parent);
        return false;
    }
    
    const HProperty* HClass::FindProperty(const std::string& PropertyName) const
    {
        for (const auto& Property : Properties)
        {
            if (Property.Name == PropertyName)
            {
                return &Property;
            }
        }
        return nullptr;
    }
    
    const HFunction* HClass::FindFunction(const std::string& FunctionName) const
    {
        for (const auto& Function : Functions)
        {
            if (Function.Name == FunctionName)
            {
                return &Function;
            }
        }
        return nullptr;
    }
    
    std::vector<HProperty> HClass::GetAllProperties() const
    {
        std::vector<HProperty> AllProperties;
        if (SuperClass)
        {
            AllProperties = SuperClass->GetAllProperties();
        }
        AllProperties.insert(AllProperties.end(), Properties.begin(), Properties.end());
        return AllProperties;
    }
    
    std::vector<HFunction> HClass::GetAllFunctions() const
    {
        std::vector<HFunction> AllFunctions;
        if (SuperClass)
        {
            AllFunctions = SuperClass->GetAllFunctions();
        }
        AllFunctions.insert(AllFunctions.end(), Functions.begin(), Functions.end());
        return AllFunctions;
    }
    
    // Implementation of HReflectionRegistry
    HReflectionRegistry& HReflectionRegistry::Get()
    {
        static HReflectionRegistry Instance;
        return Instance;
    }
    
    void HReflectionRegistry::RegisterClass(HClass* Class)
    {
        Classes[Class->Name] = std::unique_ptr<HClass>(Class);
    }
    
    HClass* HReflectionRegistry::FindClass(const std::string& ClassName)
    {
        auto It = Classes.find(ClassName);
        return (It != Classes.end()) ? It->second.get() : nullptr;
    }
    
    std::vector<std::string> HReflectionRegistry::GetAllClassNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(Classes.size());
        for (const auto& Pair : Classes)
        {
            Names.push_back(Pair.first);
        }
        return Names;
    }
    
    HObject* HReflectionRegistry::CreateObject(const std::string& ClassName)
    {
        HClass* Class = FindClass(ClassName);
        if (!Class || !Class->Constructor)
        {
            return nullptr;
        }
        return Class->Constructor();
    }
    
    void HReflectionRegistry::DestroyObject(HObject* Object)
    {
        if (!Object) return;
        
        HClass* Class = Object->GetClass();
        if (Class && Class->Destructor)
        {
            Class->Destructor(Object);
        }
    }
    
    // Template implementations
    template<typename T>
    HClass* HReflectionRegistry::GetClass()
    {
        return T::StaticClass();
    }
    
    template<typename T>
    bool HObject::IsA() const
    {
        return GetClass()->IsChildOf(T::StaticClass());
    }
    
    template<typename T>
    T* HObject::Cast()
    {
        return IsA<T>() ? static_cast<T*>(this) : nullptr;
    }
    
    template<typename T>
    const T* HObject::Cast() const
    {
        return IsA<T>() ? static_cast<const T*>(this) : nullptr;
    }
    
    // Template specializations for property access
    template<typename T>
    T TProperty<T>::Get() const
    {
        if (!Object) return T{};
        
        HClass* Class = Object->GetClass();
        if (!Class) return T{};
        
        const HProperty* Property = Class->FindProperty(Name);
        if (!Property || !Property->Getter) return T{};
        
        return *static_cast<T*>(Property->Getter(Object));
    }
    
    template<typename T>
    void TProperty<T>::Set(const T& Value)
    {
        if (!Object) return;
        
        HClass* Class = Object->GetClass();
        if (!Class) return;
        
        const HProperty* Property = Class->FindProperty(Name);
        if (!Property || !Property->Setter) return;
        
        Property->Setter(Object, const_cast<void*>(static_cast<const void*>(&Value)));
    }
    
    // Template specializations for function calls
    template<typename ReturnType, typename... Args>
    ReturnType TFunction<ReturnType, Args...>::Call(Args... args)
    {
        if (!Object) return ReturnType{};
        
        HClass* Class = Object->GetClass();
        if (!Class) return ReturnType{};
        
        const HFunction* Function = Class->FindFunction(Name);
        if (!Function || !Function->Invoker) return ReturnType{};
        
        std::vector<void*> Arguments = {static_cast<void*>(&args)...};
        void* Result = Function->Invoker(Object, Arguments);
        
        return *static_cast<ReturnType*>(Result);
    }
    
    // Global reflection initialization
    void InitializeHReflection();
    void ShutdownHReflection();
}