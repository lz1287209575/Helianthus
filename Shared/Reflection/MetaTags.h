#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <any>

namespace Helianthus::Reflection::Meta
{
    // Meta标签类型
    struct MetaTag
    {
        std::string Name;
        std::string Value;
        std::unordered_map<std::string, std::string> Parameters;
        
        MetaTag() = default;
        MetaTag(const std::string& name) : Name(name) {}
        MetaTag(const std::string& name, const std::string& value) : Name(name), Value(value) {}
        
        bool HasParameter(const std::string& key) const;
        std::string GetParameter(const std::string& key, const std::string& defaultValue = "") const;
        void SetParameter(const std::string& key, const std::string& value);
    };
    
    // Meta标签集合
    class MetaCollection
    {
    public:
        void AddTag(const MetaTag& tag);
        void AddTag(const std::string& name, const std::string& value = "");
        
        bool HasTag(const std::string& name) const;
        const MetaTag* GetTag(const std::string& name) const;
        std::vector<const MetaTag*> GetTags(const std::string& name) const;
        
        const std::vector<MetaTag>& GetAllTags() const { return Tags; }
        
        std::string ToString() const;
        
    private:
        std::vector<MetaTag> Tags;
        std::unordered_map<std::string, std::vector<size_t>> TagIndices;
    };
    
    // 反射元数据
    struct ReflectedProperty
    {
        std::string Name;
        std::string Type;
        size_t Offset;
        MetaCollection Meta;
    };
    
    struct ReflectedFunction
    {
        std::string Name;
        std::string ReturnType;
        std::vector<std::string> Parameters;
        MetaCollection Meta;
        bool bIsConst = false;
    };
    
    struct ReflectedClass
    {
        std::string Name;
        std::string SuperClassName;
        std::vector<ReflectedProperty> Properties;
        std::vector<ReflectedFunction> Functions;
        MetaCollection Meta;
    };
    
    // 预定义的Meta标签
    namespace Tags
    {
        // 属性标签
        static const char* ScriptReadable = "ScriptReadable";
        static const char* ScriptWritable = "ScriptWritable";
        static const char* BlueprintReadOnly = "BlueprintReadOnly";
        static const char* BlueprintReadWrite = "BlueprintReadWrite";
        static const char* SaveGame = "SaveGame";
        static const char* Config = "Config";
        static const char* EditAnywhere = "EditAnywhere";
        static const char* EditDefaultsOnly = "EditDefaultsOnly";
        static const char* VisibleAnywhere = "VisibleAnywhere";
        static const char* VisibleDefaultsOnly = "VisibleDefaultsOnly";
        static const char* Category = "Category";
        static const char* DisplayName = "DisplayName";
        static const char* Tooltip = "Tooltip";
        
        // 函数标签
        static const char* ScriptCallable = "ScriptCallable";
        static const char* ScriptEvent = "ScriptEvent";
        static const char* BlueprintCallable = "BlueprintCallable";
        static const char* BlueprintEvent = "BlueprintEvent";
        static const char* BlueprintPure = "BlueprintPure";
        static const char* NetMulticast = "NetMulticast";
        static const char* NetServer = "NetServer";
        static const char* NetClient = "NetClient";
        static const char* AuthorityOnly = "AuthorityOnly";
        
        // 类标签
        static const char* Scriptable = "Scriptable";
        static const char* BlueprintType = "BlueprintType";
        static const char* ConfigClass = "ConfigClass";
        static const char* DefaultConfig = "DefaultConfig";
    }
    
    // Meta解析器
    class MetaParser
    {
    public:
        static MetaCollection ParseMeta(const std::string& metaString);
        static std::string GenerateMetaString(const MetaCollection& meta);
    };
}