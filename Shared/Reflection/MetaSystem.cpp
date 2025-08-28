#include "MetaSystem.h"
#include <algorithm>

namespace Helianthus::Reflection::Meta
{
    // MetaTag实现
    bool MetaTag::HasParameter(const std::string& Key) const
    {
        return Parameters.find(Key) != Parameters.end();
    }
    
    std::string MetaTag::GetParameter(const std::string& Key, const std::string& DefaultValue) const
    {
        auto It = Parameters.find(Key);
        return It != Parameters.end() ? It->second : DefaultValue;
    }
    
    void MetaTag::SetParameter(const std::string& Key, const std::string& Value)
    {
        Parameters[Key] = Value;
    }
    
    // MetaCollection实现
    void MetaCollection::AddTag(const MetaTag& Tag)
    {
        Tags.push_back(Tag);
        TagIndices[Tag.Name].push_back(Tags.size() - 1);
    }
    
    void MetaCollection::AddTag(const std::string& Name, const std::string& Value)
    {
        AddTag(MetaTag(Name, Value));
    }
    
    bool MetaCollection::HasTag(const std::string& Name) const
    {
        return TagIndices.find(Name) != TagIndices.end();
    }
    
    const MetaTag* MetaCollection::GetTag(const std::string& Name) const
    {
        auto It = TagIndices.find(Name);
        if (It != TagIndices.end() && !It->second.empty())
        {
            return &Tags[It->second[0]];
        }
        return nullptr;
    }
    
    std::vector<const MetaTag*> MetaCollection::GetTags(const std::string& Name) const
    {
        std::vector<const MetaTag*> Result;
        auto It = TagIndices.find(Name);
        if (It != TagIndices.end())
        {
            for (size_t Index : It->second)
            {
                Result.push_back(&Tags[Index]);
            }
        }
        return Result;
    }
    
    std::string MetaCollection::ToString() const
    {
        std::ostringstream Oss;
        for (const auto& Tag : Tags)
        {
            Oss << Tag.Name;
            if (!Tag.Value.empty())
            {
                Oss << "=" << Tag.Value;
            }
            
            if (!Tag.Parameters.empty())
            {
                Oss << "(";
                bool First = true;
                for (const auto& Param : Tag.Parameters)
                {
                    if (!First) Oss << ",";
                    Oss << Param.first << "=" << Param.second;
                    First = false;
                }
                Oss << ")";
            }
            Oss << " ";
        }
        return Oss.str();
    }
    
    // MetaParser实现
    MetaCollection MetaParser::ParseMeta(const std::string& MetaString)
    {
        MetaCollection Collection;
        
        // 简单的解析器，支持格式：TagName=Value(Key1=Val1,Key2=Val2)
        std::istringstream Iss(MetaString);
        std::string Token;
        
        while (Iss >> Token)
        {
            size_t EqualPos = Token.find('=');
            size_t ParenPos = Token.find('(');
            
            std::string Name = Token.substr(0, EqualPos != std::string::npos ? EqualPos : Token.length());
            std::string Value;
            std::unordered_map<std::string, std::string> Params;
            
            if (EqualPos != std::string::npos)
            {
                if (ParenPos != std::string::npos && ParenPos > EqualPos)
                {
                    Value = Token.substr(EqualPos + 1, ParenPos - EqualPos - 1);
                    
                    // 解析参数
                    std::string ParamStr = Token.substr(ParenPos + 1, Token.length() - ParenPos - 2);
                    std::istringstream ParamIss(ParamStr);
                    std::string ParamToken;
                    
                    while (std::getline(ParamIss, ParamToken, ','))
                    {
                        size_t ParamEqual = ParamToken.find('=');
                        if (ParamEqual != std::string::npos)
                        {
                            std::string ParamName = ParamToken.substr(0, ParamEqual);
                            std::string ParamValue = ParamToken.substr(ParamEqual + 1);
                            Params[ParamName] = ParamValue;
                        }
                    }
                }
                else
                {
                    Value = Token.substr(EqualPos + 1);
                }
            }
            
            MetaTag Tag(Name, Value);
            Tag.Parameters = Params;
            Collection.AddTag(Tag);
        }
        
        return Collection;
    }
    
    std::string MetaParser::GenerateMetaString(const MetaCollection& Meta)
    {
        return Meta.ToString();
    }
    
    // ReflectionRegistry实现
    ReflectionRegistry& ReflectionRegistry::Get()
    {
        static ReflectionRegistry Instance;
        return Instance;
    }
    
    void ReflectionRegistry::RegisterClass(const ReflectedClass& Class)
    {
        Classes[Class.Name] = Class;
    }
    
    const ReflectedClass* ReflectionRegistry::GetClass(const std::string& Name) const
    {
        auto It = Classes.find(Name);
        return It != Classes.end() ? &It->second : nullptr;
    }
    
    const ReflectedProperty* ReflectionRegistry::GetProperty(const std::string& ClassName, const std::string& PropertyName) const
    {
        const ReflectedClass* Class = GetClass(ClassName);
        if (!Class) return nullptr;
        
        for (const auto& Property : Class->Properties)
        {
            if (Property.Name == PropertyName)
            {
                return &Property;
            }
        }
        return nullptr;
    }
    
    const ReflectedFunction* ReflectionRegistry::GetFunction(const std::string& ClassName, const std::string& FunctionName) const
    {
        const ReflectedClass* Class = GetClass(ClassName);
        if (!Class) return nullptr;
        
        for (const auto& Function : Class->Functions)
        {
            if (Function.Name == FunctionName)
            {
                return &Function;
            }
        }
        return nullptr;
    }
    
    std::vector<std::string> ReflectionRegistry::GetClassNames() const
    {
        std::vector<std::string> Names;
        Names.reserve(Classes.size());
        for (const auto& Pair : Classes)
        {
            Names.push_back(Pair.first);
        }
        return Names;
    }
    
    std::vector<std::string> ReflectionRegistry::GetPropertyNames(const std::string& ClassName) const
    {
        std::vector<std::string> Names;
        const ReflectedClass* Class = GetClass(ClassName);
        if (Class)
        {
            for (const auto& Property : Class->Properties)
            {
                Names.push_back(Property.Name);
            }
        }
        return Names;
    }
    
    std::vector<std::string> ReflectionRegistry::GetFunctionNames(const std::string& ClassName) const
    {
        std::vector<std::string> Names;
        const ReflectedClass* Class = GetClass(ClassName);
        if (Class)
        {
            for (const auto& Function : Class->Functions)
            {
                Names.push_back(Function.Name);
            }
        }
        return Names;
    }
}