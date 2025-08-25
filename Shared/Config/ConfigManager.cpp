#include "ConfigManager.h"
#include "JsonConfigProvider.h"
#include "Shared/Common/StructuredLogger.h"
#include <regex>
#include <filesystem>

namespace Helianthus::Config
{
    ConfigManager& ConfigManager::Instance()
    {
        static ConfigManager instance;
        return instance;
    }

    ConfigManager::~ConfigManager()
    {
        Shutdown();
    }

    bool ConfigManager::Initialize(std::unique_ptr<IConfigProvider> ProviderPtr, const ConfigManagerConfig& ConfigParam)
    {
        if (IsInitializedFlag.load())
        {
            SetLastError("ConfigManager is already initialized");
            return false;
        }

        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!ProviderPtr || !ProviderPtr->IsValid())
        {
            SetLastError("Invalid configuration provider");
            return false;
        }

        Provider = std::move(ProviderPtr);
        Config = ConfigParam;
        ShouldStop = false;

        // 初始化统计信息
        UpdateStats([](ConfigStats& Stats) {
            Stats.TotalKeys = 0;
            Stats.ReloadCount = 0;
            Stats.SaveCount = 0;
            Stats.ValidationErrors = 0;
            Stats.LastReloadTime = std::chrono::system_clock::now();
            Stats.LastSaveTime = std::chrono::system_clock::now();
        });

        // 启动热更新线程
        if (Config.EnableHotReload)
        {
            HotReloadThread = std::thread(&ConfigManager::HotReloadWorker, this);
        }

        // 启动自动保存线程
        if (Config.EnableAutoSave)
        {
            AutoSaveThread = std::thread(&ConfigManager::AutoSaveWorker, this);
        }

        IsInitializedFlag = true;

        // 记录初始化日志
        Helianthus::Common::LogFields Fields;
        Fields.AddField("config_source", Provider->GetSource());
        Fields.AddField("hot_reload_enabled", Config.EnableHotReload);
        Fields.AddField("auto_save_enabled", Config.EnableAutoSave);
        Helianthus::Common::StructuredLogger::Info("CONFIG", "Configuration manager initialized", Fields);

        return true;
    }

    void ConfigManager::Shutdown()
    {
        if (!IsInitializedFlag.load())
        {
            return;
        }

        ShouldStop = true;

        // 等待热更新线程结束
        if (HotReloadThread.joinable())
        {
            HotReloadThread.join();
        }

        // 等待自动保存线程结束
        if (AutoSaveThread.joinable())
        {
            AutoSaveThread.join();
        }

        // 清理资源
        {
            std::lock_guard<std::mutex> Lock(ProviderMutex);
            Provider.reset();
        }

        {
            std::lock_guard<std::mutex> Lock(ValidatorMutex);
            Validators.clear();
        }

        IsInitializedFlag = false;

        Helianthus::Common::StructuredLogger::Info("CONFIG", "Configuration manager shutdown");
    }

    bool ConfigManager::IsInitialized() const
    {
        return IsInitializedFlag.load();
    }

    bool ConfigManager::ReloadConfig()
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            SetLastError("Configuration provider not initialized");
            return false;
        }

        bool Success = Provider->Reload();
        if (Success)
        {
            UpdateStats([](ConfigStats& Stats) {
                Stats.ReloadCount++;
                Stats.LastReloadTime = std::chrono::system_clock::now();
            });

            Helianthus::Common::StructuredLogger::Info("CONFIG", "Configuration reloaded successfully");
        }
        else
        {
            SetLastError(Provider->GetLastError());
            Helianthus::Common::LogFields Fields;
            Fields.AddField("error", Provider->GetLastError());
            Helianthus::Common::StructuredLogger::Error("CONFIG", "Failed to reload configuration", Fields);
        }

        return Success;
    }

    std::optional<ConfigValue> ConfigManager::GetValue(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            return std::nullopt;
        }

        std::string FullKey = ApplyGlobalPrefix(Key);
        return Provider->GetValue(FullKey);
    }

    bool ConfigManager::SetValue(const std::string& Key, const ConfigValue& Value)
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            SetLastError("Configuration provider not initialized");
            return false;
        }

        // 配置验证
        if (Config.EnableConfigValidation)
        {
            std::string ValidationError;
            if (!ValidateConfig(Key, Value, ValidationError))
            {
                SetLastError("Configuration validation failed: " + ValidationError);
                
                UpdateStats([](ConfigStats& Stats) {
                    Stats.ValidationErrors++;
                });

                Helianthus::Common::LogFields Fields;
                Fields.AddField("key", Key);
                Fields.AddField("validation_error", ValidationError);
                Helianthus::Common::StructuredLogger::Warn("CONFIG", "Configuration validation failed", Fields);
                
                return false;
            }
        }

        std::string FullKey = ApplyGlobalPrefix(Key);
        bool Success = Provider->SetValue(FullKey, Value);
        
        if (!Success)
        {
            SetLastError(Provider->GetLastError());
        }

        return Success;
    }

    bool ConfigManager::HasKey(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            return false;
        }

        std::string FullKey = ApplyGlobalPrefix(Key);
        return Provider->HasKey(FullKey);
    }

    std::vector<std::string> ConfigManager::GetAllKeys() const
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            return {};
        }

        auto AllKeys = Provider->GetAllKeys();
        
        // 移除全局前缀
        std::vector<std::string> FilteredKeys;
        for (const auto& Key : AllKeys)
        {
            std::string FilteredKey = RemoveGlobalPrefix(Key);
            if (!FilteredKey.empty())
            {
                FilteredKeys.push_back(FilteredKey);
            }
        }

        return FilteredKeys;
    }

    std::unordered_map<std::string, ConfigValue> ConfigManager::GetSection(const std::string& Prefix) const
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            return {};
        }

        std::string FullPrefix = ApplyGlobalPrefix(Prefix);
        auto Section = Provider->GetSection(FullPrefix);
        
        // 移除全局前缀
        std::unordered_map<std::string, ConfigValue> FilteredSection;
        for (const auto& [Key, Value] : Section)
        {
            std::string FilteredKey = RemoveGlobalPrefix(Key);
            if (!FilteredKey.empty())
            {
                FilteredSection[FilteredKey] = Value;
            }
        }

        return FilteredSection;
    }

    void ConfigManager::RegisterChangeCallback(const std::string& Key, ConfigChangeCallback Callback)
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (Provider)
        {
            std::string FullKey = ApplyGlobalPrefix(Key);
            Provider->RegisterChangeCallback(FullKey, std::move(Callback));
        }
    }

    void ConfigManager::UnregisterChangeCallback(const std::string& Key)
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (Provider)
        {
            std::string FullKey = ApplyGlobalPrefix(Key);
            Provider->UnregisterChangeCallback(FullKey);
        }
    }

    void ConfigManager::RegisterValidator(const std::string& KeyPattern, ConfigValidator Validator)
    {
        std::lock_guard<std::mutex> Lock(ValidatorMutex);
        Validators[KeyPattern] = std::move(Validator);
    }

    void ConfigManager::UnregisterValidator(const std::string& KeyPattern)
    {
        std::lock_guard<std::mutex> Lock(ValidatorMutex);
        Validators.erase(KeyPattern);
    }

    bool ConfigManager::SaveConfig(const std::string& Destination)
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        
        if (!Provider)
        {
            SetLastError("Configuration provider not initialized");
            return false;
        }

        bool Success = Provider->Save(Destination);
        if (Success)
        {
            UpdateStats([](ConfigStats& Stats) {
                Stats.SaveCount++;
                Stats.LastSaveTime = std::chrono::system_clock::now();
            });

            Helianthus::Common::StructuredLogger::Info("CONFIG", "Configuration saved successfully");
        }
        else
        {
            SetLastError(Provider->GetLastError());
            Helianthus::Common::LogFields Fields;
            Fields.AddField("error", Provider->GetLastError());
            Helianthus::Common::StructuredLogger::Error("CONFIG", "Failed to save configuration", Fields);
        }

        return Success;
    }

    std::string ConfigManager::GetConfigSource() const
    {
        std::lock_guard<std::mutex> Lock(ProviderMutex);
        return Provider ? Provider->GetSource() : "";
    }

    std::string ConfigManager::GetLastError() const
    {
        std::lock_guard<std::mutex> Lock(ErrorMutex);
        return LastError;
    }

    ConfigManager::ConfigStats ConfigManager::GetStats() const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        ConfigStats CurrentStats = Stats;
        
        // 更新总键数
        {
            std::lock_guard<std::mutex> ProviderLock(ProviderMutex);
            if (Provider)
            {
                CurrentStats.TotalKeys = Provider->GetAllKeys().size();
            }
        }
        
        return CurrentStats;
    }

    bool ConfigManager::ValidateAllConfigs(std::vector<std::string>& ValidationErrors)
    {
        ValidationErrors.clear();
        
        if (!Config.EnableConfigValidation)
        {
            return true;
        }

        auto AllKeys = GetAllKeys();
        bool AllValid = true;

        for (const auto& Key : AllKeys)
        {
            auto Value = GetValue(Key);
            if (Value.has_value())
            {
                std::string ErrorMessage;
                if (!ValidateConfig(Key, *Value, ErrorMessage))
                {
                    ValidationErrors.push_back("Key '" + Key + "': " + ErrorMessage);
                    AllValid = false;
                }
            }
        }

        if (!AllValid)
        {
            UpdateStats([&](ConfigStats& Stats) {
                Stats.ValidationErrors += ValidationErrors.size();
            });
        }

        return AllValid;
    }

    void ConfigManager::SetGlobalPrefix(const std::string& Prefix)
    {
        std::lock_guard<std::mutex> Lock(PrefixMutex);
        GlobalPrefix = Prefix;
    }

    std::string ConfigManager::GetGlobalPrefix() const
    {
        std::lock_guard<std::mutex> Lock(PrefixMutex);
        return GlobalPrefix;
    }

    // 私有方法实现
    void ConfigManager::HotReloadWorker()
    {
        while (!ShouldStop.load())
        {
            try
            {
                std::this_thread::sleep_for(Config.HotReloadInterval);
                
                if (ShouldStop.load())
                {
                    break;
                }

                // 检查配置文件是否修改
                bool ShouldReload = false;
                {
                    std::lock_guard<std::mutex> Lock(ProviderMutex);
                    if (Provider)
                    {
                        // 如果是JsonConfigProvider，检查文件修改时间
                        auto* JsonProvider = dynamic_cast<JsonConfigProvider*>(Provider.get());
                        if (JsonProvider && JsonProvider->IsFileModified())
                        {
                            ShouldReload = true;
                        }
                    }
                }

                if (ShouldReload)
                {
                    Helianthus::Common::StructuredLogger::Debug("CONFIG", "Configuration file modified, reloading...");
                    ReloadConfig();
                }
            }
            catch (const std::exception& e)
            {
                Helianthus::Common::LogFields Fields;
                Fields.AddField("error", e.what());
                Helianthus::Common::StructuredLogger::Error("CONFIG", "Error in hot reload worker", Fields);
            }
        }
    }

    void ConfigManager::AutoSaveWorker()
    {
        while (!ShouldStop.load())
        {
            try
            {
                std::this_thread::sleep_for(Config.AutoSaveInterval);
                
                if (ShouldStop.load())
                {
                    break;
                }

                Helianthus::Common::StructuredLogger::Debug("CONFIG", "Auto-saving configuration...");
                SaveConfig();
            }
            catch (const std::exception& e)
            {
                Helianthus::Common::LogFields Fields;
                Fields.AddField("error", e.what());
                Helianthus::Common::StructuredLogger::Error("CONFIG", "Error in auto save worker", Fields);
            }
        }
    }

    bool ConfigManager::ValidateConfig(const std::string& Key, const ConfigValue& Value, std::string& ErrorMessage)
    {
        std::lock_guard<std::mutex> Lock(ValidatorMutex);
        
        for (const auto& [Pattern, Validator] : Validators)
        {
            try
            {
                std::regex PatternRegex(Pattern);
                if (std::regex_match(Key, PatternRegex))
                {
                    if (!Validator(Key, Value, ErrorMessage))
                    {
                        return false;
                    }
                }
            }
            catch (const std::exception& e)
            {
                ErrorMessage = "Validator pattern error: " + std::string(e.what());
                return false;
            }
        }

        return true;
    }

    std::string ConfigManager::ApplyGlobalPrefix(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(PrefixMutex);
        if (GlobalPrefix.empty())
        {
            return Key;
        }
        return GlobalPrefix + "." + Key;
    }

    std::string ConfigManager::RemoveGlobalPrefix(const std::string& Key) const
    {
        std::lock_guard<std::mutex> Lock(PrefixMutex);
        if (GlobalPrefix.empty())
        {
            return Key;
        }
        
        std::string Prefix = GlobalPrefix + ".";
        if (Key.find(Prefix) == 0)
        {
            return Key.substr(Prefix.length());
        }
        
        return Key;
    }

    void ConfigManager::UpdateStats(const std::function<void(ConfigStats&)>& Updater) const
    {
        std::lock_guard<std::mutex> Lock(StatsMutex);
        Updater(Stats);
    }

    void ConfigManager::SetLastError(const std::string& Error) const
    {
        std::lock_guard<std::mutex> Lock(ErrorMutex);
        LastError = Error;
    }

    // ConfigFactory实现
    std::unique_ptr<IConfigProvider> ConfigFactory::CreateProvider(ConfigFormat Format)
    {
        switch (Format)
        {
            case ConfigFormat::JSON:
                return std::make_unique<JsonConfigProvider>();
            case ConfigFormat::YAML:
                // TODO: 实现YAML提供者
                return nullptr;
            default:
                return nullptr;
        }
    }

    std::unique_ptr<IConfigProvider> ConfigFactory::CreateProviderFromFile(const std::string& FilePath)
    {
        if (FilePath.empty())
        {
            return nullptr;
        }

        std::filesystem::path Path(FilePath);
        std::string Extension = Path.extension().string();
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

        ConfigFormat Format = ConfigFormat::JSON; // 默认JSON
        if (Extension == ".json")
        {
            Format = ConfigFormat::JSON;
        }
        else if (Extension == ".yaml" || Extension == ".yml")
        {
            Format = ConfigFormat::YAML;
        }

        auto Provider = CreateProvider(Format);
        if (Provider && Provider->Load(FilePath))
        {
            return Provider;
        }

        return nullptr;
    }

    // 全局配置函数实现
    namespace GlobalConfig
    {
        bool Initialize(const std::string& ConfigFile, ConfigFormat Format, const ConfigManagerConfig& Config)
        {
            std::unique_ptr<IConfigProvider> Provider;
            
            if (Format == ConfigFormat::AUTO_DETECT)
            {
                Provider = ConfigFactory::CreateProviderFromFile(ConfigFile);
            }
            else
            {
                Provider = ConfigFactory::CreateProvider(Format);
                if (Provider && !Provider->Load(ConfigFile))
                {
                    return false;
                }
            }

            if (!Provider)
            {
                return false;
            }

            return ConfigManager::Instance().Initialize(std::move(Provider), Config);
        }

        bool Set(const std::string& Key, const ConfigValue& Value)
        {
            return ConfigManager::Instance().SetValue(Key, Value);
        }

        bool Has(const std::string& Key)
        {
            return ConfigManager::Instance().HasKey(Key);
        }

        bool Reload()
        {
            return ConfigManager::Instance().ReloadConfig();
        }

        bool Save()
        {
            return ConfigManager::Instance().SaveConfig();
        }

        void Shutdown()
        {
            ConfigManager::Instance().Shutdown();
        }
    }

} // namespace Helianthus::Config
