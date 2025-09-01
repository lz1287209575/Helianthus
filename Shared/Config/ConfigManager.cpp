#include "ConfigManager.h"
#include "../Common/Utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Helianthus::Config
{

// 全局配置管理器实例
std::unique_ptr<ConfigManager> GlobalConfigManager;

// ConfigValue 实现
std::string ConfigValue::AsString() const
{
    switch (Type)
    {
    case ConfigValueType::STRING:
        return StringValue;
    case ConfigValueType::INTEGER:
        return std::to_string(IntValue);
    case ConfigValueType::FLOAT:
        {
            // 使用更精确的字符串格式化，避免显示过多小数位
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << FloatValue;
            std::string result = oss.str();
            // 移除末尾的0
            while (result.back() == '0' && result.find('.') != std::string::npos) {
                result.pop_back();
            }
            if (result.back() == '.') {
                result.pop_back();
            }
            return result;
        }
    case ConfigValueType::BOOLEAN:
        return BoolValue ? "true" : "false";
    case ConfigValueType::ARRAY:
        {
            std::string Result = "[";
            for (size_t i = 0; i < ArrayValue.size(); ++i)
            {
                if (i > 0) Result += ",";
                Result += "\"" + ArrayValue[i] + "\"";
            }
            Result += "]";
            return Result;
        }
    case ConfigValueType::OBJECT:
        {
            std::string Result = "{";
            bool First = true;
            for (const auto& [Key, Value] : ObjectValue)
            {
                if (!First) Result += ",";
                Result += "\"" + Key + "\":\"" + Value + "\"";
                First = false;
            }
            Result += "}";
            return Result;
        }
    default:
        return "";
    }
}

int64_t ConfigValue::AsInt() const
{
    switch (Type)
    {
    case ConfigValueType::INTEGER:
        return IntValue;
    case ConfigValueType::STRING:
        try
        {
            return std::stoll(StringValue);
        }
        catch (...)
        {
            return 0;
        }
    case ConfigValueType::FLOAT:
        return static_cast<int64_t>(FloatValue);
    case ConfigValueType::BOOLEAN:
        return BoolValue ? 1 : 0;
    default:
        return 0;
    }
}

double ConfigValue::AsFloat() const
{
    switch (Type)
    {
    case ConfigValueType::FLOAT:
        return FloatValue;
    case ConfigValueType::INTEGER:
        return static_cast<double>(IntValue);
    case ConfigValueType::STRING:
        try
        {
            return std::stod(StringValue);
        }
        catch (...)
        {
            return 0.0;
        }
    case ConfigValueType::BOOLEAN:
        return BoolValue ? 1.0 : 0.0;
    default:
        return 0.0;
    }
}

bool ConfigValue::AsBool() const
{
    switch (Type)
    {
    case ConfigValueType::BOOLEAN:
        return BoolValue;
    case ConfigValueType::INTEGER:
        return IntValue != 0;
    case ConfigValueType::FLOAT:
        return FloatValue != 0.0;
    case ConfigValueType::STRING:
        {
            std::string Lower = StringValue;
            std::transform(Lower.begin(), Lower.end(), Lower.begin(), ::tolower);
            return Lower == "true" || Lower == "1" || Lower == "yes" || Lower == "on";
        }
    default:
        return false;
    }
}

std::vector<std::string> ConfigValue::AsArray() const
{
    if (Type == ConfigValueType::ARRAY)
    {
        return ArrayValue;
    }
    return {};
}

std::unordered_map<std::string, std::string> ConfigValue::AsObject() const
{
    if (Type == ConfigValueType::OBJECT)
    {
        return ObjectValue;
    }
    return {};
}

bool ConfigValue::IsValid() const
{
    switch (Type)
    {
    case ConfigValueType::STRING:
        return !StringValue.empty();
    case ConfigValueType::INTEGER:
        return true; // 所有整数都是有效的
    case ConfigValueType::FLOAT:
        return !std::isnan(FloatValue) && !std::isinf(FloatValue);
    case ConfigValueType::BOOLEAN:
        return true; // 所有布尔值都是有效的
    case ConfigValueType::ARRAY:
        return true; // 空数组也是有效的
    case ConfigValueType::OBJECT:
        return true; // 空对象也是有效的
    default:
        return false;
    }
}

std::string ConfigValue::ToString() const
{
    return AsString();
}

// ConfigManager 实现
ConfigManager::ConfigManager()
{
}

ConfigManager::~ConfigManager()
{
    Shutdown();
}

ConfigManager::ConfigManager(ConfigManager&& Other) noexcept
    : ConfigItems(std::move(Other.ConfigItems))
    , InitializedFlag(Other.InitializedFlag.load())
    , ConfigLocked(Other.ConfigLocked.load())
    , ConfigPath(std::move(Other.ConfigPath))
    , ConfigFiles(std::move(Other.ConfigFiles))
    , Validators(std::move(Other.Validators))
    , ChangeCallbacks(std::move(Other.ChangeCallbacks))
    , GlobalChangeCallbacks(std::move(Other.GlobalChangeCallbacks))
    , ModifiedKeys(std::move(Other.ModifiedKeys))
{
}

ConfigManager& ConfigManager::operator=(ConfigManager&& Other) noexcept
{
    if (this != &Other)
    {
        Shutdown();
        ConfigItems = std::move(Other.ConfigItems);
        InitializedFlag = Other.InitializedFlag.load();
        ConfigLocked = Other.ConfigLocked.load();
        ConfigPath = std::move(Other.ConfigPath);
        ConfigFiles = std::move(Other.ConfigFiles);
        Validators = std::move(Other.Validators);
        ChangeCallbacks = std::move(Other.ChangeCallbacks);
        GlobalChangeCallbacks = std::move(Other.GlobalChangeCallbacks);
        ModifiedKeys = std::move(Other.ModifiedKeys);
    }
    return *this;
}

bool ConfigManager::Initialize(const std::string& InConfigPath)
{
    if (InitializedFlag.load())
    {
        return true;
    }

    ConfigPath = InConfigPath;
    
    // 创建配置目录
    try
    {
        std::filesystem::create_directories(ConfigPath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create config directory: " << e.what() << std::endl;
        return false;
    }

    // 加载默认配置
    LoadDefaultConfig();

    InitializedFlag.store(true);
    std::cout << "ConfigManager initialized with path: " << ConfigPath << std::endl;
    return true;
}

void ConfigManager::Shutdown()
{
    if (!InitializedFlag.load())
    {
        return;
    }

    InitializedFlag.store(false);
    std::cout << "ConfigManager shutdown" << std::endl;
}

bool ConfigManager::IsInitialized() const
{
    return InitializedFlag.load();
}

bool ConfigManager::LoadFromFile(const std::string& FilePath)
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    std::ifstream File(FilePath);
    if (!File.is_open())
    {
        std::cerr << "Failed to open config file: " << FilePath << std::endl;
        return false;
    }

    std::string Line;
    size_t LineNumber = 0;
    bool Success = true;

    while (std::getline(File, Line))
    {
        LineNumber++;
        
        // 跳过空行和注释
        if (Line.empty() || Line[0] == '#' || Line[0] == ';')
        {
            continue;
        }

        std::string Key;
        ConfigValue Value;
        if (!ParseConfigLine(Line, Key, Value))
        {
            std::cout << "Invalid config line " << LineNumber << " in " << FilePath << ": " << Line << std::endl;
            Success = false;
            continue;
        }

        if (!SetValue(Key, Value))
        {
            std::cout << "Failed to set config " << Key << " = " << Value.ToString() << std::endl;
            Success = false;
        }
    }

    ConfigFiles.push_back(FilePath);
    std::cout << "Loaded config from file: " << FilePath << std::endl;
    return Success;
}

bool ConfigManager::SaveToFile(const std::string& FilePath) const
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    std::ofstream File(FilePath);
    if (!File.is_open())
    {
        std::cerr << "Failed to create config file: " << FilePath << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    for (const auto& [Key, Item] : ConfigItems)
    {
        if (!Item.Description.empty())
        {
            File << "# " << Item.Description << "\n";
        }
        File << Key << " = " << Item.Value.ToString() << "\n\n";
    }

    std::cout << "Saved config to file: " << FilePath << std::endl;
    return true;
}

bool ConfigManager::LoadFromEnvironment()
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    for (const auto& [Key, Item] : ConfigItems)
    {
        std::string EnvKey = "HELIANTHUS_" + Key;
        std::transform(EnvKey.begin(), EnvKey.end(), EnvKey.begin(), ::toupper);
        
        const char* EnvValue = std::getenv(EnvKey.c_str());
        if (EnvValue)
        {
            SetString(Key, EnvValue);
        }
    }

    std::cout << "Loaded config from environment variables" << std::endl;
    return true;
}

bool ConfigManager::LoadFromCommandLine(int Argc, char* Argv[])
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    for (int i = 1; i < Argc; ++i)
    {
        std::string Arg = Argv[i];
        if (Arg.substr(0, 2) == "--")
        {
            std::string Key = Arg.substr(2);
            std::string Value = "true"; // 默认值

            // 检查是否有等号分隔的值
            size_t EqualPos = Key.find('=');
            if (EqualPos != std::string::npos)
            {
                Value = Key.substr(EqualPos + 1);
                Key = Key.substr(0, EqualPos);
            }
            else if (i + 1 < Argc && Argv[i + 1][0] != '-')
            {
                // 下一个参数是值
                Value = Argv[i + 1];
                i++; // 跳过下一个参数
            }

            SetString(Key, Value);
        }
    }

    std::cout << "Loaded config from command line arguments" << std::endl;
    return true;
}

bool ConfigManager::SetValue(const std::string& Key, const ConfigValue& Value)
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    if (ConfigLocked.load())
    {
        std::cout << "Config is locked, cannot set value: " << Key << std::endl;
        return false;
    }

    // 首先检查原始键的有效性
    if (!IsValidKey(Key))
    {
        std::cerr << "Invalid config key: " << Key << std::endl;
        return false;
    }
    
    std::string NormalizedKey = NormalizeKey(Key);

    if (!ValidateValue(NormalizedKey, Value))
    {
        std::cerr << "Invalid config value for key: " << Key << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    ConfigValue OldValue;
    auto It = ConfigItems.find(NormalizedKey);
    if (It != ConfigItems.end())
    {
        OldValue = It->second.Value;
        It->second.Value = Value;
    }
    else
    {
        ConfigItems[NormalizedKey] = ConfigItem(NormalizedKey, Value);
    }

    ModifiedKeys.push_back(NormalizedKey);
    Lock.~lock_guard();

    NotifyChangeCallbacks(NormalizedKey, OldValue, Value);
    std::cout << "Set config " << NormalizedKey << " = " << Value.ToString() << std::endl;
    return true;
}

bool ConfigManager::SetString(const std::string& Key, const std::string& Value)
{
    return SetValue(Key, ConfigValue(Value));
}

bool ConfigManager::SetInt(const std::string& Key, int64_t Value)
{
    return SetValue(Key, ConfigValue(Value));
}

bool ConfigManager::SetFloat(const std::string& Key, double Value)
{
    return SetValue(Key, ConfigValue(Value));
}

bool ConfigManager::SetBool(const std::string& Key, bool Value)
{
    return SetValue(Key, ConfigValue(Value));
}

bool ConfigManager::SetArray(const std::string& Key, const std::vector<std::string>& Value)
{
    return SetValue(Key, ConfigValue(Value));
}

bool ConfigManager::SetObject(const std::string& Key, const std::unordered_map<std::string, std::string>& Value)
{
    return SetValue(Key, ConfigValue(Value));
}

ConfigValue ConfigManager::GetValue(const std::string& Key) const
{
    if (!InitializedFlag.load())
    {
        return ConfigValue();
    }

    std::string NormalizedKey = NormalizeKey(Key);
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    auto It = ConfigItems.find(NormalizedKey);
    if (It != ConfigItems.end())
    {
        return It->second.Value;
    }

    return ConfigValue();
}

std::string ConfigManager::GetString(const std::string& Key, const std::string& Default) const
{
    ConfigValue Value = GetValue(Key);
    if (Value.Type == ConfigValueType::STRING)
    {
        return Value.StringValue.empty() ? Default : Value.StringValue;
    }
    else if (Value.Type != ConfigValueType::STRING)
    {
        // 对于非字符串类型，使用 AsString() 转换
        return Value.AsString();
    }
    return Default;
}

int64_t ConfigManager::GetInt(const std::string& Key, int64_t Default) const
{
    ConfigValue Value = GetValue(Key);
    if (Value.Type != ConfigValueType::STRING || !Value.StringValue.empty())
    {
        return Value.AsInt();
    }
    return Default;
}

double ConfigManager::GetFloat(const std::string& Key, double Default) const
{
    ConfigValue Value = GetValue(Key);
    if (Value.Type != ConfigValueType::STRING || !Value.StringValue.empty())
    {
        return Value.AsFloat();
    }
    return Default;
}

bool ConfigManager::GetBool(const std::string& Key, bool Default) const
{
    ConfigValue Value = GetValue(Key);
    if (Value.Type != ConfigValueType::STRING || !Value.StringValue.empty())
    {
        return Value.AsBool();
    }
    return Default;
}

std::vector<std::string> ConfigManager::GetArray(const std::string& Key) const
{
    ConfigValue Value = GetValue(Key);
    return Value.AsArray();
}

std::unordered_map<std::string, std::string> ConfigManager::GetObject(const std::string& Key) const
{
    ConfigValue Value = GetValue(Key);
    return Value.AsObject();
}

bool ConfigManager::AddConfigItem(const ConfigItem& Item)
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    std::string NormalizedKey = NormalizeKey(Item.Key);
    if (!IsValidKey(NormalizedKey))
    {
        std::cerr << "Invalid config key: " << Item.Key << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    // 创建一个新的 ConfigItem，使用标准化的键
    ConfigItem NormalizedItem = Item;
    NormalizedItem.Key = NormalizedKey;
    ConfigItems[NormalizedKey] = NormalizedItem;
    
    std::cout << "Added config item: " << NormalizedKey << std::endl;
    return true;
}

bool ConfigManager::RemoveConfigItem(const std::string& Key)
{
    if (!InitializedFlag.load())
    {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    auto It = ConfigItems.find(NormalizedKey);
    if (It != ConfigItems.end())
    {
        ConfigItems.erase(It);
        std::cout << "Removed config item: " << NormalizedKey << std::endl;
        return true;
    }

    return false;
}

bool ConfigManager::HasConfigItem(const std::string& Key) const
{
    if (!InitializedFlag.load())
    {
        return false;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    return ConfigItems.find(NormalizedKey) != ConfigItems.end();
}

ConfigItem ConfigManager::GetConfigItem(const std::string& Key) const
{
    if (!InitializedFlag.load())
    {
        return ConfigItem();
    }

    std::string NormalizedKey = NormalizeKey(Key);
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    auto It = ConfigItems.find(NormalizedKey);
    if (It != ConfigItems.end())
    {
        return It->second;
    }

    return ConfigItem();
}

std::vector<std::string> ConfigManager::GetAllKeys() const
{
    if (!InitializedFlag.load())
    {
        return {};
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    std::vector<std::string> Keys;
    Keys.reserve(ConfigItems.size());
    
    for (const auto& [Key, _] : ConfigItems)
    {
        Keys.push_back(Key);
    }

    return Keys;
}

bool ConfigManager::ValidateConfig() const
{
    if (!InitializedFlag.load())
    {
        return false;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    for (const auto& [Key, Item] : ConfigItems)
    {
        if (!ValidateConfigItemUnlocked(Key))
        {
            return false;
        }
    }

    return true;
}

bool ConfigManager::ValidateConfigItem(const std::string& Key) const
{
    if (!InitializedFlag.load())
    {
        return false;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    return ValidateConfigItemUnlocked(Key);
}

bool ConfigManager::ValidateConfigItemUnlocked(const std::string& Key) const
{
    if (!InitializedFlag.load())
    {
        return false;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    
    auto It = ConfigItems.find(NormalizedKey);
    if (It == ConfigItems.end())
    {
        return false;
    }

    return ValidateValue(NormalizedKey, It->second.Value);
}

void ConfigManager::AddValidator(const std::string& Key, ConfigValidator Validator)
{
    if (!InitializedFlag.load())
    {
        return;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    Validators[NormalizedKey] = Validator;
    std::cout << "Added validator for config key: " << NormalizedKey << std::endl;
}

void ConfigManager::RemoveValidator(const std::string& Key)
{
    if (!InitializedFlag.load())
    {
        return;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    Validators.erase(NormalizedKey);
    std::cout << "Removed validator for config key: " << NormalizedKey << std::endl;
}

void ConfigManager::AddChangeCallback(const std::string& Key, ConfigChangeCallback Callback)
{
    if (!InitializedFlag.load())
    {
        return;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    ChangeCallbacks[NormalizedKey].push_back(Callback);
    std::cout << "Added change callback for config key: " << NormalizedKey << std::endl;
}

void ConfigManager::RemoveChangeCallback(const std::string& Key)
{
    if (!InitializedFlag.load())
    {
        return;
    }

    std::string NormalizedKey = NormalizeKey(Key);
    ChangeCallbacks.erase(NormalizedKey);
    std::cout << "Removed change callback for config key: " << NormalizedKey << std::endl;
}

void ConfigManager::AddGlobalChangeCallback(ConfigChangeCallback Callback)
{
    if (!InitializedFlag.load())
    {
        return;
    }

    GlobalChangeCallbacks.push_back(Callback);
    std::cout << "Added global change callback" << std::endl;
}

void ConfigManager::RemoveGlobalChangeCallback()
{
    if (!InitializedFlag.load())
    {
        return;
    }

    GlobalChangeCallbacks.clear();
    std::cout << "Removed global change callback" << std::endl;
}

void ConfigManager::LoadDefaultConfig()
{
    // 加载基本配置
    SetString("app.name", "Helianthus");
    SetString("app.version", "1.0.0");
    SetString("app.environment", "development");
    SetBool("app.debug", true);
    SetInt("app.port", 8080);
    SetString("app.host", "localhost");
    
    std::cout << "Loaded default configuration" << std::endl;
}

void ConfigManager::LoadMessageQueueConfig()
{
    // 消息队列配置
    SetInt("messagequeue.max_size", 10000);
    SetInt("messagequeue.max_size_bytes", 100 * 1024 * 1024); // 100MB
    SetInt("messagequeue.max_consumers", 100);
    SetInt("messagequeue.max_producers", 100);
    SetInt("messagequeue.message_ttl_ms", 300000); // 5分钟
    SetInt("messagequeue.queue_ttl_ms", 0); // 永不过期
    SetBool("messagequeue.enable_dead_letter", true);
    SetString("messagequeue.dead_letter_queue", "dead_letter");
    SetInt("messagequeue.max_retries", 3);
    SetInt("messagequeue.retry_delay_ms", 1000);
    SetBool("messagequeue.enable_retry_backoff", true);
    SetFloat("messagequeue.retry_backoff_multiplier", 2.0);
    SetInt("messagequeue.max_retry_delay_ms", 60000);
    SetInt("messagequeue.dead_letter_ttl_ms", 86400000); // 24小时
    SetBool("messagequeue.enable_priority", false);
    SetBool("messagequeue.enable_batching", true);
    SetInt("messagequeue.batch_size", 100);
    SetInt("messagequeue.batch_timeout_ms", 1000);
    
    std::cout << "Loaded message queue configuration" << std::endl;
}

void ConfigManager::LoadNetworkConfig()
{
    // 网络配置
    SetInt("network.max_connections", 1000);
    SetInt("network.connection_timeout_ms", 30000);
    SetInt("network.read_timeout_ms", 60000);
    SetInt("network.write_timeout_ms", 60000);
    SetInt("network.keep_alive_interval_ms", 30000);
    SetInt("network.max_message_size", 10 * 1024 * 1024); // 10MB
    SetBool("network.enable_compression", true);
    SetBool("network.enable_encryption", false);
    SetString("network.compression_algorithm", "gzip");
    SetString("network.encryption_algorithm", "aes-256-gcm");
    SetInt("network.thread_pool_size", 4);
    SetInt("network.max_pending_requests", 1000);
    
    std::cout << "Loaded network configuration" << std::endl;
}

void ConfigManager::LoadLoggingConfig()
{
    // 日志配置
    SetString("logging.level", "info");
    SetString("logging.format", "json");
    SetString("logging.output", "console");
    SetString("logging.file_path", "logs/helianthus.log");
    SetInt("logging.max_file_size_mb", 100);
    SetInt("logging.max_files", 10);
    SetBool("logging.enable_rotation", true);
    SetBool("logging.enable_timestamp", true);
    SetBool("logging.enable_thread_id", true);
    SetBool("logging.enable_color", true);
    
    std::cout << "Loaded logging configuration" << std::endl;
}

void ConfigManager::LoadMonitoringConfig()
{
    // 监控配置
    SetBool("monitoring.enable_metrics", true);
    SetInt("monitoring.metrics_port", 9090);
    SetString("monitoring.metrics_path", "/metrics");
    SetBool("monitoring.enable_health_check", true);
    SetInt("monitoring.health_check_interval_ms", 30000);
    SetBool("monitoring.enable_tracing", false);
    SetString("monitoring.tracing_endpoint", "http://localhost:14268/api/traces");
    SetBool("monitoring.enable_profiling", false);
    SetInt("monitoring.profiling_port", 6060);
    
    std::cout << "Loaded monitoring configuration" << std::endl;
}

std::string ConfigManager::ExportToJson() const
{
    if (!InitializedFlag.load())
    {
        return "{}";
    }

    std::string Json = "{\n";
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    bool First = true;
    for (const auto& [Key, Item] : ConfigItems)
    {
        if (!First) Json += ",\n";
        
        // 根据值的类型正确格式化JSON
        std::string ValueStr;
        switch (Item.Value.Type)
        {
        case ConfigValueType::STRING:
            ValueStr = "\"" + Item.Value.AsString() + "\"";
            break;
        case ConfigValueType::BOOLEAN:
            ValueStr = Item.Value.AsString(); // "true" 或 "false"
            break;
        case ConfigValueType::ARRAY:
        case ConfigValueType::OBJECT:
            ValueStr = Item.Value.AsString(); // 已经是JSON格式
            break;
        default:
            ValueStr = Item.Value.AsString(); // 数字类型
            break;
        }
        
        Json += "  \"" + Key + "\": " + ValueStr;
        First = false;
    }
    
    Json += "\n}";
    return Json;
}

std::string ConfigManager::ExportToYaml() const
{
    if (!InitializedFlag.load())
    {
        return "";
    }

    std::string Yaml;
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    for (const auto& [Key, Item] : ConfigItems)
    {
        if (!Item.Description.empty())
        {
            Yaml += "# " + Item.Description + "\n";
        }
        Yaml += Key + ": " + Item.Value.ToString() + "\n";
    }
    
    return Yaml;
}

std::string ConfigManager::ExportToIni() const
{
    if (!InitializedFlag.load())
    {
        return "";
    }

    std::string Ini;
    std::lock_guard<std::mutex> Lock(ConfigMutex);
    
    for (const auto& [Key, Item] : ConfigItems)
    {
        if (!Item.Description.empty())
        {
            Ini += "; " + Item.Description + "\n";
        }
        Ini += Key + " = " + Item.Value.ToString() + "\n";
    }
    
    return Ini;
}

bool ConfigManager::ImportFromJson(const std::string& JsonData)
{
    // 简单的JSON解析实现
    // 这里应该使用专门的JSON库
    std::cout << "JSON import not implemented" << std::endl;
    return false;
}

bool ConfigManager::ImportFromYaml(const std::string& YamlData)
{
    // 简单的YAML解析实现
    // 这里应该使用专门的YAML库
    std::cout << "YAML import not implemented" << std::endl;
    return false;
}

bool ConfigManager::ImportFromIni(const std::string& IniData)
{
    std::istringstream Stream(IniData);
    std::string Line;
    bool Success = true;

    while (std::getline(Stream, Line))
    {
        if (Line.empty() || Line[0] == ';' || Line[0] == '#')
        {
            continue;
        }

        std::string Key;
        ConfigValue Value;
        if (!ParseConfigLine(Line, Key, Value))
        {
            Success = false;
            continue;
        }

        if (!SetValue(Key, Value))
        {
            Success = false;
        }
    }

    return Success;
}

size_t ConfigManager::GetConfigItemCount() const
{
    if (!InitializedFlag.load())
    {
        return 0;
    }

    std::lock_guard<std::mutex> Lock(ConfigMutex);
    return ConfigItems.size();
}

std::vector<std::string> ConfigManager::GetModifiedKeys() const
{
    if (!InitializedFlag.load())
    {
        return {};
    }

    return ModifiedKeys;
}

void ConfigManager::ClearModifiedFlags()
{
    if (!InitializedFlag.load())
    {
        return;
    }

    ModifiedKeys.clear();
    std::cout << "Cleared modified flags" << std::endl;
}

void ConfigManager::LockConfig()
{
    ConfigLocked.store(true);
    std::cout << "Config locked" << std::endl;
}

void ConfigManager::UnlockConfig()
{
    ConfigLocked.store(false);
    std::cout << "Config unlocked" << std::endl;
}

bool ConfigManager::IsConfigLocked() const
{
    return ConfigLocked.load();
}

bool ConfigManager::ReloadConfig()
{
    if (!InitializedFlag.load())
    {
        return false;
    }

    bool Success = true;
    for (const auto& FilePath : ConfigFiles)
    {
        if (!LoadFromFile(FilePath))
        {
            Success = false;
        }
    }

    std::cout << "Reloaded config files" << std::endl;
    return Success;
}

// 私有方法实现
bool ConfigManager::ValidateValue(const std::string& Key, const ConfigValue& Value) const
{
    auto It = Validators.find(Key);
    if (It != Validators.end())
    {
        return It->second(Key, Value);
    }
    return Value.IsValid();
}

void ConfigManager::NotifyChangeCallbacks(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)
{
    // 通知特定键的回调
    auto It = ChangeCallbacks.find(Key);
    if (It != ChangeCallbacks.end())
    {
        for (const auto& Callback : It->second)
        {
            try
            {
                Callback(Key, OldValue, NewValue);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Exception in config change callback: " << e.what() << std::endl;
            }
        }
    }

    // 通知全局回调
    for (const auto& Callback : GlobalChangeCallbacks)
    {
        try
        {
            Callback(Key, OldValue, NewValue);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception in global config change callback: " << e.what() << std::endl;
        }
    }
}

bool ConfigManager::ParseConfigLine(const std::string& Line, std::string& Key, ConfigValue& Value)
{
    size_t EqualPos = Line.find('=');
    if (EqualPos == std::string::npos)
    {
        return false;
    }

    Key = Line.substr(0, EqualPos);
    std::string ValueStr = Line.substr(EqualPos + 1);

    // 去除前后空格
    Key = Helianthus::Common::Utils::Trim(Key);
    ValueStr = Helianthus::Common::Utils::Trim(ValueStr);

    if (Key.empty())
    {
        return false;
    }

    // 解析值
    if (ValueStr.empty())
    {
        Value = ConfigValue("");
    }
    else if (ValueStr == "true" || ValueStr == "false")
    {
        Value = ConfigValue(ValueStr == "true");
    }
    else if (ValueStr.find('.') != std::string::npos)
    {
        try
        {
            Value = ConfigValue(std::stod(ValueStr));
        }
        catch (...)
        {
            Value = ConfigValue(ValueStr);
        }
    }
    else
    {
        try
        {
            Value = ConfigValue(static_cast<int64_t>(std::stoll(ValueStr)));
        }
        catch (...)
        {
            Value = ConfigValue(ValueStr);
        }
    }

    return true;
}

std::string ConfigManager::EscapeString(const std::string& Str) const
{
    std::string Escaped;
    Escaped.reserve(Str.length() * 2);
    
    for (char c : Str)
    {
        switch (c)
        {
        case '\\':
            Escaped += "\\\\";
            break;
        case '"':
            Escaped += "\\\"";
            break;
        case '\n':
            Escaped += "\\n";
            break;
        case '\r':
            Escaped += "\\r";
            break;
        case '\t':
            Escaped += "\\t";
            break;
        default:
            Escaped += c;
            break;
        }
    }
    
    return Escaped;
}

std::string ConfigManager::UnescapeString(const std::string& Str) const
{
    std::string Unescaped;
    Unescaped.reserve(Str.length());
    
    for (size_t i = 0; i < Str.length(); ++i)
    {
        if (Str[i] == '\\' && i + 1 < Str.length())
        {
            switch (Str[i + 1])
            {
            case '\\':
                Unescaped += '\\';
                break;
            case '"':
                Unescaped += '"';
                break;
            case 'n':
                Unescaped += '\n';
                break;
            case 'r':
                Unescaped += '\r';
                break;
            case 't':
                Unescaped += '\t';
                break;
            default:
                Unescaped += Str[i + 1];
                break;
            }
            i++; // 跳过下一个字符
        }
        else
        {
            Unescaped += Str[i];
        }
    }
    
    return Unescaped;
}

bool ConfigManager::IsValidKey(const std::string& Key) const
{
    if (Key.empty())
    {
        return false;
    }

    // 检查是否只包含字母、数字、下划线和点
    for (char c : Key)
    {
        if (!std::isalnum(c) && c != '_' && c != '.')
        {
            return false;
        }
    }

    return true;
}

std::string ConfigManager::NormalizeKey(const std::string& Key) const
{
    std::string Normalized = Key;
    
    // 转换为小写
    std::transform(Normalized.begin(), Normalized.end(), Normalized.begin(), ::tolower);
    
    // 替换空格为下划线
    std::replace(Normalized.begin(), Normalized.end(), ' ', '_');
    
    return Normalized;
}

// 全局配置访问函数实现
namespace Global
{

bool InitializeConfig(const std::string& ConfigPath)
{
    if (GlobalConfigManager)
    {
        return true;
    }

    GlobalConfigManager = std::make_unique<ConfigManager>();
    return GlobalConfigManager->Initialize(ConfigPath);
}

void ShutdownConfig()
{
    if (GlobalConfigManager)
    {
        GlobalConfigManager->Shutdown();
        GlobalConfigManager.reset();
    }
}

std::string GetString(const std::string& Key, const std::string& Default)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->GetString(Key, Default);
    }
    return Default;
}

int64_t GetInt(const std::string& Key, int64_t Default)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->GetInt(Key, Default);
    }
    return Default;
}

double GetFloat(const std::string& Key, double Default)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->GetFloat(Key, Default);
    }
    return Default;
}

bool GetBool(const std::string& Key, bool Default)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->GetBool(Key, Default);
    }
    return Default;
}

bool SetString(const std::string& Key, const std::string& Value)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->SetString(Key, Value);
    }
    return false;
}

bool SetInt(const std::string& Key, int64_t Value)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->SetInt(Key, Value);
    }
    return false;
}

bool SetFloat(const std::string& Key, double Value)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->SetFloat(Key, Value);
    }
    return false;
}

bool SetBool(const std::string& Key, bool Value)
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->SetBool(Key, Value);
    }
    return false;
}

bool ValidateConfig()
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->ValidateConfig();
    }
    return false;
}

bool ReloadConfig()
{
    if (GlobalConfigManager)
    {
        return GlobalConfigManager->ReloadConfig();
    }
    return false;
}

} // namespace Global

// ConfigTemplate 实现
void ConfigTemplate::LoadMessageQueueDefaults(ConfigManager& Manager)
{
    Manager.LoadMessageQueueConfig();
}

void ConfigTemplate::LoadNetworkDefaults(ConfigManager& Manager)
{
    Manager.LoadNetworkConfig();
}

void ConfigTemplate::LoadLoggingDefaults(ConfigManager& Manager)
{
    Manager.LoadLoggingConfig();
}

void ConfigTemplate::LoadMonitoringDefaults(ConfigManager& Manager)
{
    Manager.LoadMonitoringConfig();
}

void ConfigTemplate::LoadSecurityDefaults(ConfigManager& Manager)
{
    // 安全配置
    Manager.SetBool("security.enable_ssl", false);
    Manager.SetString("security.cert_file", "certs/server.crt");
    Manager.SetString("security.key_file", "certs/server.key");
    Manager.SetString("security.ca_file", "certs/ca.crt");
    Manager.SetBool("security.verify_peer", true);
    Manager.SetInt("security.session_timeout_ms", 3600000); // 1小时
    Manager.SetString("security.cipher_suite", "TLS_AES_256_GCM_SHA384");
    Manager.SetInt("security.key_size", 256);
    
    std::cout << "Loaded security configuration" << std::endl;
}

void ConfigTemplate::LoadPerformanceDefaults(ConfigManager& Manager)
{
    // 性能配置
    Manager.SetInt("performance.thread_pool_size", 4);
    Manager.SetInt("performance.max_connections", 1000);
    Manager.SetInt("performance.connection_timeout_ms", 30000);
    Manager.SetInt("performance.read_buffer_size", 8192);
    Manager.SetInt("performance.write_buffer_size", 8192);
    Manager.SetBool("performance.enable_compression", true);
    Manager.SetString("performance.compression_level", "6");
    Manager.SetBool("performance.enable_caching", true);
    Manager.SetInt("performance.cache_size", 1000);
    Manager.SetInt("performance.cache_ttl_ms", 300000); // 5分钟
    
    std::cout << "Loaded performance configuration" << std::endl;
}

} // namespace Helianthus::Config
