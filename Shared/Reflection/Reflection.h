#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <iostream>
#include <typeindex>
#include <type_traits>

namespace Helianthus::Reflection
{
    // 反射标志
    enum class HClassFlags : uint32_t
    {
        None = 0,
        HELIANTHUS_CLASS = 1 << 0,
        Abstract = 1 << 1,
        Final = 1 << 2,
        BlueprintType = 1 << 3,
        Blueprintable = 1 << 4,
        Scriptable = 1 << 5,
    };

    enum class HPropertyFlags : uint32_t
    {
        None = 0,
        HELIANTHUS_PROPERTY = 1 << 0,
        ReadOnly = 1 << 1,
        WriteOnly = 1 << 2,
        BlueprintReadOnly = 1 << 3,
        BlueprintReadWrite = 1 << 4,
        EditAnywhere = 1 << 5,
        EditDefaultsOnly = 1 << 6,
        SaveGame = 1 << 7,
        Replicated = 1 << 8,
    };

    enum class HFunctionFlags : uint32_t
    {
        None = 0,
        HELIANTHUS_FUNCTION = 1 << 0,
        Public = 1 << 1,
        Private = 1 << 2,
        Protected = 1 << 3,
        Static = 1 << 4,
        Const = 1 << 5,
        BlueprintCallable = 1 << 6,
        BlueprintEvent = 1 << 7,
        BlueprintPure = 1 << 8,
    };

    // 参数信息
    struct HParameterInfo
    {
        std::string ParameterName;
        std::string ParameterType;
    };

    // 属性信息
    struct HPropertyInfo
    {
        std::string PropertyName;
        std::string PropertyType;
        HPropertyFlags PropertyFlags = HPropertyFlags::None;
        std::string Category;
        std::string DisplayName;
        std::string ToolTip;
        std::string MetaData;
        
        // 访问器
        std::function<void*(void*)> Getter;
        std::function<void(void*, void*)> Setter;
        
        // 默认值
        std::string DefaultValue;
        std::string MinValue;
        std::string MaxValue;
        
        // 数组信息
        bool IsArray = false;
        std::string ArrayType;
    };

    // 函数信息
    struct HFunctionInfo
    {
        std::string FunctionName;
        std::string ReturnType;
        HFunctionFlags FunctionFlags = HFunctionFlags::None;
        std::string Category;
        std::string DisplayName;
        std::string ToolTip;
        std::string MetaData;
        
        // 参数
        std::vector<HParameterInfo> Parameters;
        
        // 调用器
        std::function<void*(void*, const std::vector<void*>&)> Invoker;
    };

    // 类信息
    struct HClassInfo
    {
        std::string ClassName;
        std::string BaseClassName;
        std::type_index TypeIndex = typeid(void);
        HClassFlags ClassFlags = HClassFlags::None;
        std::vector<std::string> Categories;
        std::string DisplayName;
        std::string ToolTip;
        std::string MetaData;
        
        // 构造函数和析构函数
        std::function<void*(void*)> Constructor;
        std::function<void(void*)> Destructor;
        
        // 属性和方法
        std::vector<HPropertyInfo> Properties;
        std::vector<HFunctionInfo> Functions;
        
        // 自动注册
        bool AutoRegister() const { return true; }
    };

    // 枚举信息
    struct HEnumInfo
    {
        std::string EnumName;
        std::unordered_map<std::string, int32_t> EnumValues;
        std::string Category;
        std::string DisplayName;
        std::string ToolTip;
        std::string MetaData;
    };

    // 基础对象类
    class HObject
    {
    public:
        virtual ~HObject() = default;
        
        // 运行时类型信息
        virtual const std::type_info& GetTypeInfo() const = 0;
        virtual const std::string& GetClassName() const = 0;
        
        // 属性访问
        virtual void* GetProperty(const std::string& PropertyName) = 0;
        virtual void SetProperty(const std::string& PropertyName, void* Value) = 0;
        
        // 方法调用
        virtual void* CallFunction(const std::string& FunctionName, const std::vector<void*>& Arguments) = 0;
    };

    // 类型特征辅助
    template<typename T>
    struct TypeTraits
    {
        static constexpr bool IsReflected = false;
        static constexpr const char* ClassName = "Unknown";
        static constexpr const char* BaseClassName = "HObject";
    };

    // 反射系统
    class HelianthusReflectionSystem
    {
    public:
        static HelianthusReflectionSystem& GetInstance();
        
        // 注册
        void RegisterHClass(const HClassInfo& ClassInfo);
        void RegisterHProperty(const std::string& ClassName, const HPropertyInfo& PropertyInfo);
        void RegisterHFunction(const std::string& ClassName, const HFunctionInfo& FunctionInfo);
        void RegisterHEnum(const HEnumInfo& EnumInfo);
        
        // 查询
        const HClassInfo* GetHClassInfo(const std::string& ClassName) const;
        const HPropertyInfo* GetHPropertyInfo(const std::string& ClassName, const std::string& PropertyName) const;
        const HFunctionInfo* GetHFunctionInfo(const std::string& ClassName, const std::string& FunctionName) const;
        const HEnumInfo* GetHEnumInfo(const std::string& EnumName) const;
        
        // 对象操作
        void* CreateHObject(const std::string& ClassName);
        void DestroyHObject(const std::string& ClassName, void* Object);
        void* GetHProperty(void* Object, const std::string& PropertyName);
        void SetHProperty(void* Object, const std::string& PropertyName, void* Value);
        void* CallHFunction(void* Object, const std::string& FunctionName, const std::vector<void*>& Arguments);
        
        // 类型检查
        bool IsHClass(const std::string& ClassName) const;
        bool IsHProperty(const std::string& ClassName, const std::string& PropertyName) const;
        bool IsHFunction(const std::string& ClassName, const std::string& FunctionName) const;
        bool IsHEnum(const std::string& EnumName) const;
        
        // 枚举
        std::vector<std::string> GetAllHClassNames() const;
        std::vector<std::string> GetAllHPropertyNames(const std::string& ClassName) const;
        std::vector<std::string> GetAllHFunctionNames(const std::string& ClassName) const;
        std::vector<std::string> GetAllHEnumNames() const;
        
        // 统计
        size_t GetRegisteredHClassCount() const;
        size_t GetRegisteredHEnumCount() const;
        
        // 获取所有信息
        std::vector<HClassInfo> GetAllHClassInfos() const;
        std::vector<HEnumInfo> GetAllHEnumInfos() const;
        
        // 代码生成
        std::string GenerateHClassCode(const std::string& ClassName) const;
        std::string GenerateHPropertyCode(const std::string& ClassName, const std::string& PropertyName) const;
        std::string GenerateHFunctionCode(const std::string& ClassName, const std::string& FunctionName) const;
        
        // 脚本绑定
        std::string GenerateScriptBindings(const std::string& Language = "lua") const;
        bool SaveScriptBindings(const std::string& FilePath, const std::string& Language = "lua") const;

    private:
        HelianthusReflectionSystem() = default;
        ~HelianthusReflectionSystem() = default;
        HelianthusReflectionSystem(const HelianthusReflectionSystem&) = delete;
        HelianthusReflectionSystem& operator=(const HelianthusReflectionSystem&) = delete;
        
        std::unordered_map<std::string, HClassInfo> HClasses;
        std::unordered_map<std::string, std::unordered_map<std::string, HPropertyInfo>> HProperties;
        std::unordered_map<std::string, std::unordered_map<std::string, HFunctionInfo>> HFunctions;
        std::unordered_map<std::string, HEnumInfo> HEnums;
    };

    // 全局实例
    extern HelianthusReflectionSystem* GHelianthusReflectionSystem;

    // 初始化函数
    void InitializeHelianthusReflectionSystem();
    void ShutdownHelianthusReflectionSystem();

} // namespace Helianthus::Reflection

// 简化的运行时反射宏
#define HELIANTHUS_CLASS(ClassName, BaseClass) \
    class ClassName : public BaseClass

#define HELIANTHUS_PROPERTY(PropertyName, PropertyType) \
    PropertyType PropertyName;

#define HELIANTHUS_FUNCTION(FunctionName, ReturnType, ...) \
    ReturnType FunctionName(__VA_ARGS__);

#define HELIANTHUS_ENUM(EnumName) \
    enum class EnumName : int32_t

#define HELIANTHUS_GENERATED_BODY() \
    public: \
        virtual const std::type_info& GetTypeInfo() const override { return typeid(*this); } \
        virtual const std::string& GetClassName() const override { static std::string Name = typeid(*this).name(); return Name; } \
        virtual void* GetProperty(const std::string& PropertyName) override; \
        virtual void SetProperty(const std::string& PropertyName, void* Value) override; \
        virtual void* CallFunction(const std::string& FunctionName, const std::vector<void*>& Arguments) override;
