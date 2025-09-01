#pragma once

#include "../Common/Types.h"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Helianthus::Config
{

// 配置值类型
enum class ConfigValueType : uint8_t
{
    STRING = 0,
    INTEGER = 1,
    FLOAT = 2,
    BOOLEAN = 3,
    ARRAY = 4,
    OBJECT = 5
};

// 配置值结构
struct ConfigValue
{
    ConfigValueType Type = ConfigValueType::STRING;
    std::string StringValue;
    int64_t IntValue = 0;
    double FloatValue = 0.0;
    bool BoolValue = false;
    std::vector<std::string> ArrayValue;
    std::unordered_map<std::string, std::string> ObjectValue;

    ConfigValue() = default;
    
    // 构造函数
    ConfigValue(const std::string& Value) : Type(ConfigValueType::STRING), StringValue(Value) {}
    ConfigValue(int64_t Value) : Type(ConfigValueType::INTEGER), IntValue(Value) {}
    ConfigValue(double Value) : Type(ConfigValueType::FLOAT), FloatValue(Value) {}
    ConfigValue(bool Value) : Type(ConfigValueType::BOOLEAN), BoolValue(Value) {}
    ConfigValue(const std::vector<std::string>& Value) : Type(ConfigValueType::ARRAY), ArrayValue(Value) {}
    ConfigValue(const std::unordered_map<std::string, std::string>& Value) : Type(ConfigValueType::OBJECT), ObjectValue(Value) {}

    // 类型转换方法
    std::string AsString() const;
    int64_t AsInt() const;
    double AsFloat() const;
    bool AsBool() const;
    std::vector<std::string> AsArray() const;
    std::unordered_map<std::string, std::string> AsObject() const;

    // 验证方法
    bool IsValid() const;
    std::string ToString() const;
};

// 配置项结构
struct ConfigItem
{
    std::string Key;
    ConfigValue Value;
    std::string Description;
    std::string DefaultValue;
    bool Required = false;
    bool ReadOnly = false;
    std::vector<std::string> AllowedValues;
    std::function<bool(const ConfigValue&)> Validator;

    ConfigItem() = default;
    ConfigItem(const std::string& InKey, const ConfigValue& InValue, const std::string& InDescription = "")
        : Key(InKey), Value(InValue), Description(InDescription) {}
};

// 配置变更回调
using ConfigChangeCallback = std::function<void(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)>;

// 配置验证器
using ConfigValidator = std::function<bool(const std::string& Key, const ConfigValue& Value)>;

// 配置管理器类
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager();

    // 禁用拷贝，允许移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&& Other) noexcept;
    ConfigManager& operator=(ConfigManager&& Other) noexcept;

    // 初始化和生命周期
    bool Initialize(const std::string& ConfigPath = "config/");
    void Shutdown();
    bool IsInitialized() const;

    // 配置文件操作
    bool LoadFromFile(const std::string& FilePath);
    bool SaveToFile(const std::string& FilePath) const;
    bool LoadFromEnvironment();
    bool LoadFromCommandLine(int Argc, char* Argv[]);

    // 配置项操作
    bool SetValue(const std::string& Key, const ConfigValue& Value);
    bool SetString(const std::string& Key, const std::string& Value);
    bool SetInt(const std::string& Key, int64_t Value);
    bool SetFloat(const std::string& Key, double Value);
    bool SetBool(const std::string& Key, bool Value);
    bool SetArray(const std::string& Key, const std::vector<std::string>& Value);
    bool SetObject(const std::string& Key, const std::unordered_map<std::string, std::string>& Value);

    ConfigValue GetValue(const std::string& Key) const;
    std::string GetString(const std::string& Key, const std::string& Default = "") const;
    int64_t GetInt(const std::string& Key, int64_t Default = 0) const;
    double GetFloat(const std::string& Key, double Default = 0.0) const;
    bool GetBool(const std::string& Key, bool Default = false) const;
    std::vector<std::string> GetArray(const std::string& Key) const;
    std::unordered_map<std::string, std::string> GetObject(const std::string& Key) const;

    // 配置项管理
    bool AddConfigItem(const ConfigItem& Item);
    bool RemoveConfigItem(const std::string& Key);
    bool HasConfigItem(const std::string& Key) const;
    ConfigItem GetConfigItem(const std::string& Key) const;
    std::vector<std::string> GetAllKeys() const;

    // 配置验证
    bool ValidateConfig() const;
    bool ValidateConfigItem(const std::string& Key) const;
    bool ValidateConfigItemUnlocked(const std::string& Key) const;
    void AddValidator(const std::string& Key, ConfigValidator Validator);
    void RemoveValidator(const std::string& Key);

    // 配置变更通知
    void AddChangeCallback(const std::string& Key, ConfigChangeCallback Callback);
    void RemoveChangeCallback(const std::string& Key);
    void AddGlobalChangeCallback(ConfigChangeCallback Callback);
    void RemoveGlobalChangeCallback();

    // 配置模板
    void LoadDefaultConfig();
    void LoadMessageQueueConfig();
    void LoadNetworkConfig();
    void LoadLoggingConfig();
    void LoadMonitoringConfig();

    // 配置导出
    std::string ExportToJson() const;
    std::string ExportToYaml() const;
    std::string ExportToIni() const;

    // 配置导入
    bool ImportFromJson(const std::string& JsonData);
    bool ImportFromYaml(const std::string& YamlData);
    bool ImportFromIni(const std::string& IniData);

    // 配置统计
    size_t GetConfigItemCount() const;
    std::vector<std::string> GetModifiedKeys() const;
    void ClearModifiedFlags();
    
    // 重新加载配置
    bool ReloadConfig();

    // 配置锁定
    void LockConfig();
    void UnlockConfig();
    bool IsConfigLocked() const;

private:
    // 内部辅助方法
    bool ValidateValue(const std::string& Key, const ConfigValue& Value) const;
    void NotifyChangeCallbacks(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue);
    bool ParseConfigLine(const std::string& Line, std::string& Key, ConfigValue& Value);
    std::string EscapeString(const std::string& Str) const;
    std::string UnescapeString(const std::string& Str) const;
    bool IsValidKey(const std::string& Key) const;
    std::string NormalizeKey(const std::string& Key) const;

private:
    // 配置存储
    std::unordered_map<std::string, ConfigItem> ConfigItems;
    mutable std::mutex ConfigMutex;

    // 状态管理
    std::atomic<bool> InitializedFlag = false;
    std::atomic<bool> ConfigLocked = false;

    // 配置路径
    std::string ConfigPath;
    std::vector<std::string> ConfigFiles;

    // 验证器
    std::unordered_map<std::string, ConfigValidator> Validators;

    // 变更回调
    std::unordered_map<std::string, std::vector<ConfigChangeCallback>> ChangeCallbacks;
    std::vector<ConfigChangeCallback> GlobalChangeCallbacks;

    // 修改跟踪
    std::vector<std::string> ModifiedKeys;
};

// 全局配置管理器实例
extern std::unique_ptr<ConfigManager> GlobalConfigManager;

// 便捷的全局配置访问函数
namespace Global
{
    bool InitializeConfig(const std::string& ConfigPath = "config/");
    void ShutdownConfig();
    
    // 便捷的配置访问
    std::string GetString(const std::string& Key, const std::string& Default = "");
    int64_t GetInt(const std::string& Key, int64_t Default = 0);
    double GetFloat(const std::string& Key, double Default = 0.0);
    bool GetBool(const std::string& Key, bool Default = false);
    
    // 配置设置
    bool SetString(const std::string& Key, const std::string& Value);
    bool SetInt(const std::string& Key, int64_t Value);
    bool SetFloat(const std::string& Key, double Value);
    bool SetBool(const std::string& Key, bool Value);
    
    // 配置验证
    bool ValidateConfig();
    bool ReloadConfig();
}

// 配置模板类
class ConfigTemplate
{
public:
    static void LoadMessageQueueDefaults(ConfigManager& Manager);
    static void LoadNetworkDefaults(ConfigManager& Manager);
    static void LoadLoggingDefaults(ConfigManager& Manager);
    static void LoadMonitoringDefaults(ConfigManager& Manager);
    static void LoadSecurityDefaults(ConfigManager& Manager);
    static void LoadPerformanceDefaults(ConfigManager& Manager);
};

} // namespace Helianthus::Config
