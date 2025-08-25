#include "JsonConfigProvider.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Helianthus::Config
{
    JsonConfigProvider::JsonConfigProvider()
        : JsonData(std::make_unique<nlohmann::json>()),
          IsValidFlag(false),
          FileWatchingEnabled(false)
    {
    }

    JsonConfigProvider::~JsonConfigProvider() = default;

    bool JsonConfigProvider::Load(const std::string& Source)
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        SourcePath = Source;
        LastError.clear();
        IsValidFlag = false;

        try
        {
            if (Source.empty())
            {
                LastError = "Empty source path provided";
                return false;
            }

            // 检查文件是否存在
            if (!std::filesystem::exists(Source))
            {
                LastError = "Configuration file does not exist: " + Source;
                return false;
            }

            std::ifstream File(Source);
            if (!File.is_open())
            {
                LastError = "Failed to open configuration file: " + Source;
                return false;
            }

            // 解析JSON
            nlohmann::json NewJsonData;
            File >> NewJsonData;
            File.close();

            *JsonData = std::move(NewJsonData);
            IsValidFlag = true;
            UpdateLastModifiedTime();

            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            LastError = "JSON parsing error: " + std::string(e.what());
            return false;
        }
        catch (const std::exception& e)
        {
            LastError = "Error loading configuration: " + std::string(e.what());
            return false;
        }
    }

    bool JsonConfigProvider::LoadFromString(const std::string& JsonString)
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        LastError.clear();
        IsValidFlag = false;

        try
        {
            nlohmann::json NewJsonData = nlohmann::json::parse(JsonString);
            *JsonData = std::move(NewJsonData);
            IsValidFlag = true;
            SourcePath = "<string>";
            
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            LastError = "JSON parsing error: " + std::string(e.what());
            return false;
        }
    }

    bool JsonConfigProvider::Reload()
    {
        if (SourcePath.empty() || SourcePath == "<string>")
        {
            LastError = "No valid source path for reloading";
            return false;
        }

        return Load(SourcePath);
    }

    std::optional<ConfigValue> JsonConfigProvider::GetValue(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        if (!IsValidFlag)
        {
            return std::nullopt;
        }

        auto JsonValue = GetJsonValue(Key);
        if (!JsonValue.has_value())
        {
            return std::nullopt;
        }

        try
        {
            return JsonValueToConfigValue(*JsonValue);
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }

    bool JsonConfigProvider::SetValue(const std::string& Key, const ConfigValue& Value)
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        if (!IsValidFlag)
        {
            LastError = "Configuration is not valid";
            return false;
        }

        try
        {
            // 获取旧值用于通知（直接调用内部方法避免死锁）
            auto OldValue = GetJsonValue(Key);
            ConfigValue OldConfigValue;
            bool HasOldValue = false;
            if (OldValue.has_value())
            {
                try
                {
                    OldConfigValue = JsonValueToConfigValue(*OldValue);
                    HasOldValue = true;
                }
                catch (const std::exception&)
                {
                    // 忽略旧值转换错误
                }
            }
            
            nlohmann::json JsonValue = ConfigValueToJsonValue(Value);
            bool Success = SetJsonValue(Key, JsonValue);
            
            if (Success && HasOldValue)
            {
                NotifyConfigChange(Key, OldConfigValue, Value);
            }
            
            return Success;
        }
        catch (const std::exception& e)
        {
            LastError = "Error setting value: " + std::string(e.what());
            return false;
        }
    }

    bool JsonConfigProvider::HasKey(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        return GetJsonValue(Key).has_value();
    }

    std::vector<std::string> JsonConfigProvider::GetAllKeys() const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        std::vector<std::string> Keys;
        if (IsValidFlag)
        {
            CollectKeysRecursive(*JsonData, "", Keys);
        }
        return Keys;
    }

    std::unordered_map<std::string, ConfigValue> JsonConfigProvider::GetSection(const std::string& Prefix) const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        std::unordered_map<std::string, ConfigValue> Section;
        
        if (!IsValidFlag)
        {
            return Section;
        }

        // 直接收集键，避免调用GetAllKeys()造成死锁
        std::vector<std::string> Keys;
        CollectKeysRecursive(*JsonData, "", Keys);
        
        for (const auto& Key : Keys)
        {
            if (Key.find(Prefix) == 0)
            {
                auto JsonValue = GetJsonValue(Key);
                if (JsonValue.has_value())
                {
                    try
                    {
                        auto ConfigValue = JsonValueToConfigValue(*JsonValue);
                        Section[Key] = ConfigValue;
                    }
                    catch (const std::exception&)
                    {
                        // 忽略转换错误
                    }
                }
            }
        }

        return Section;
    }

    void JsonConfigProvider::RegisterChangeCallback(const std::string& Key, ConfigChangeCallback Callback)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        ChangeCallbacks[Key] = std::move(Callback);
    }

    void JsonConfigProvider::UnregisterChangeCallback(const std::string& Key)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        ChangeCallbacks.erase(Key);
    }

    bool JsonConfigProvider::Save(const std::string& Destination)
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        if (!IsValidFlag)
        {
            LastError = "Configuration is not valid";
            return false;
        }

        std::string SavePath = Destination.empty() ? SourcePath : Destination;
        if (SavePath.empty() || SavePath == "<string>")
        {
            LastError = "No valid destination path for saving";
            return false;
        }

        try
        {
            std::ofstream File(SavePath);
            if (!File.is_open())
            {
                LastError = "Failed to open file for writing: " + SavePath;
                return false;
            }

            File << JsonData->dump(4); // 4 spaces indentation
            File.close();

            if (Destination.empty())
            {
                UpdateLastModifiedTime();
            }

            return true;
        }
        catch (const std::exception& e)
        {
            LastError = "Error saving configuration: " + std::string(e.what());
            return false;
        }
    }

    std::string JsonConfigProvider::GetSource() const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        return SourcePath;
    }

    bool JsonConfigProvider::IsValid() const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        return IsValidFlag;
    }

    std::string JsonConfigProvider::GetLastError() const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        return LastError;
    }

    const nlohmann::json& JsonConfigProvider::GetRawJson() const
    {
        return *JsonData;
    }

    void JsonConfigProvider::SetFileWatching(bool Enable)
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        FileWatchingEnabled = Enable;
    }

    bool JsonConfigProvider::IsFileModified() const
    {
        std::lock_guard<std::mutex> Lock(DataMutex);
        
        if (!FileWatchingEnabled || SourcePath.empty() || SourcePath == "<string>")
        {
            return false;
        }

        try
        {
            if (!std::filesystem::exists(SourcePath))
            {
                return false;
            }

            auto CurrentModifiedTime = std::filesystem::last_write_time(SourcePath);
            return CurrentModifiedTime != LastModifiedTime;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    // 私有方法实现
    ConfigValue JsonConfigProvider::JsonValueToConfigValue(const nlohmann::json& JsonValue) const
    {
        if (JsonValue.is_string())
        {
            return JsonValue.get<std::string>();
        }
        else if (JsonValue.is_boolean())
        {
            return JsonValue.get<bool>();
        }
        else if (JsonValue.is_number_integer())
        {
            auto IntValue = JsonValue.get<int64_t>();
            if (IntValue >= INT32_MIN && IntValue <= INT32_MAX)
            {
                return static_cast<int32_t>(IntValue);
            }
            return IntValue;
        }
        else if (JsonValue.is_number_unsigned())
        {
            auto UIntValue = JsonValue.get<uint64_t>();
            if (UIntValue <= UINT32_MAX)
            {
                return static_cast<uint32_t>(UIntValue);
            }
            return UIntValue;
        }
        else if (JsonValue.is_number_float())
        {
            return JsonValue.get<double>();
        }
        else
        {
            // 对于其他类型（数组、对象等），转换为字符串
            return JsonValue.dump();
        }
    }

    nlohmann::json JsonConfigProvider::ConfigValueToJsonValue(const ConfigValue& Value) const
    {
        return std::visit([](const auto& Val) {
            return nlohmann::json(Val);
        }, Value);
    }

    void JsonConfigProvider::CollectKeysRecursive(const nlohmann::json& JsonObj, const std::string& Prefix, std::vector<std::string>& Keys) const
    {
        if (JsonObj.is_object())
        {
            for (auto it = JsonObj.begin(); it != JsonObj.end(); ++it)
            {
                std::string FullKey = Prefix.empty() ? it.key() : Prefix + "." + it.key();
                
                if (it.value().is_object())
                {
                    CollectKeysRecursive(it.value(), FullKey, Keys);
                }
                else
                {
                    Keys.push_back(FullKey);
                }
            }
        }
    }

    std::optional<nlohmann::json> JsonConfigProvider::GetJsonValue(const std::string& Key) const
    {
        auto KeyParts = SplitKey(Key);
        if (KeyParts.empty())
        {
            return std::nullopt;
        }

        const nlohmann::json* Current = JsonData.get();
        
        for (const auto& Part : KeyParts)
        {
            if (!Current->is_object() || !Current->contains(Part))
            {
                return std::nullopt;
            }
            Current = &(*Current)[Part];
        }

        return *Current;
    }

    bool JsonConfigProvider::SetJsonValue(const std::string& Key, const nlohmann::json& Value)
    {
        auto KeyParts = SplitKey(Key);
        if (KeyParts.empty())
        {
            return false;
        }

        nlohmann::json* Current = JsonData.get();
        
        // 导航到父节点
        for (size_t i = 0; i < KeyParts.size() - 1; ++i)
        {
            const auto& Part = KeyParts[i];
            
            if (!Current->is_object())
            {
                *Current = nlohmann::json::object();
            }
            
            if (!Current->contains(Part))
            {
                (*Current)[Part] = nlohmann::json::object();
            }
            
            Current = &(*Current)[Part];
        }

        // 设置最终值
        if (!Current->is_object())
        {
            *Current = nlohmann::json::object();
        }
        
        (*Current)[KeyParts.back()] = Value;
        return true;
    }

    void JsonConfigProvider::NotifyConfigChange(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)
    {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        
        auto it = ChangeCallbacks.find(Key);
        if (it != ChangeCallbacks.end() && it->second)
        {
            try
            {
                it->second(Key, OldValue, NewValue);
            }
            catch (const std::exception&)
            {
                // 忽略回调中的异常，避免影响主程序
            }
        }
    }

    void JsonConfigProvider::UpdateLastModifiedTime()
    {
        if (!SourcePath.empty() && SourcePath != "<string>")
        {
            try
            {
                if (std::filesystem::exists(SourcePath))
                {
                    LastModifiedTime = std::filesystem::last_write_time(SourcePath);
                }
            }
            catch (const std::exception&)
            {
                // 忽略文件时间获取错误
            }
        }
    }

    std::vector<std::string> JsonConfigProvider::SplitKey(const std::string& Key) const
    {
        std::vector<std::string> Parts;
        std::stringstream Ss(Key);
        std::string Part;
        
        while (std::getline(Ss, Part, '.'))
        {
            if (!Part.empty())
            {
                Parts.push_back(Part);
            }
        }
        
        return Parts;
    }

} // namespace Helianthus::Config
