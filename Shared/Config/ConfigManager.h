#pragma once

#include "IConfigProvider.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace Helianthus::Config
{
    // 配置管理器配置
    struct ConfigManagerConfig
    {
        bool EnableHotReload = true;                    // 是否启用热更新
        std::chrono::milliseconds HotReloadInterval = std::chrono::milliseconds(1000); // 热更新检查间隔
        bool EnableAutoSave = false;                    // 是否启用自动保存
        std::chrono::milliseconds AutoSaveInterval = std::chrono::minutes(5);          // 自动保存间隔
        bool EnableConfigValidation = true;            // 是否启用配置验证
    };

    // 配置验证器类型
    using ConfigValidator = std::function<bool(const std::string& Key, const ConfigValue& Value, std::string& ErrorMessage)>;

    // 配置管理器 - 单例模式
    class ConfigManager
    {
    public:
        // 获取单例实例
        static ConfigManager& Instance();

        // 禁用拷贝和赋值
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        // 初始化配置管理器
        bool Initialize(std::unique_ptr<IConfigProvider> Provider, const ConfigManagerConfig& Config = ConfigManagerConfig());

        // 关闭配置管理器
        void Shutdown();

        // 检查是否已初始化
        bool IsInitialized() const;

        // 重新加载配置
        bool ReloadConfig();

        // 获取配置值
        template<typename T>
        T GetValue(const std::string& Key, const T& DefaultValue = T{}) const
        {
            std::lock_guard<std::mutex> Lock(ProviderMutex);
            if (Provider)
            {
                return Provider->GetValue<T>(Key, DefaultValue);
            }
            return DefaultValue;
        }

        // 获取配置值（可选类型）
        std::optional<ConfigValue> GetValue(const std::string& Key) const;

        // 设置配置值
        bool SetValue(const std::string& Key, const ConfigValue& Value);

        // 检查配置键是否存在
        bool HasKey(const std::string& Key) const;

        // 获取所有配置键
        std::vector<std::string> GetAllKeys() const;

        // 获取配置节
        std::unordered_map<std::string, ConfigValue> GetSection(const std::string& Prefix) const;

        // 注册配置变更回调
        void RegisterChangeCallback(const std::string& Key, ConfigChangeCallback Callback);

        // 注销配置变更回调
        void UnregisterChangeCallback(const std::string& Key);

        // 注册配置验证器
        void RegisterValidator(const std::string& KeyPattern, ConfigValidator Validator);

        // 注销配置验证器
        void UnregisterValidator(const std::string& KeyPattern);

        // 保存配置
        bool SaveConfig(const std::string& Destination = "");

        // 获取配置源信息
        std::string GetConfigSource() const;

        // 获取最后错误信息
        std::string GetLastError() const;

        // 获取配置统计信息
        struct ConfigStats
        {
            size_t TotalKeys = 0;
            size_t ReloadCount = 0;
            size_t SaveCount = 0;
            size_t ValidationErrors = 0;
            std::chrono::system_clock::time_point LastReloadTime;
            std::chrono::system_clock::time_point LastSaveTime;
        };
        ConfigStats GetStats() const;

        // 手动触发配置验证
        bool ValidateAllConfigs(std::vector<std::string>& ValidationErrors);

        // 设置全局配置前缀（用于命名空间隔离）
        void SetGlobalPrefix(const std::string& Prefix);

        // 获取全局配置前缀
        std::string GetGlobalPrefix() const;

    private:
        ConfigManager() = default;
        ~ConfigManager();

        // 配置提供者
        std::unique_ptr<IConfigProvider> Provider;
        mutable std::mutex ProviderMutex;

        // 配置管理器配置
        ConfigManagerConfig Config;

        // 热更新线程
        std::thread HotReloadThread;
        std::atomic<bool> ShouldStop{false};
        std::atomic<bool> IsInitializedFlag{false};

        // 自动保存线程
        std::thread AutoSaveThread;

        // 配置验证器
        std::unordered_map<std::string, ConfigValidator> Validators;
        mutable std::mutex ValidatorMutex;

        // 统计信息
        mutable ConfigStats Stats;
        mutable std::mutex StatsMutex;

        // 全局前缀
        std::string GlobalPrefix;
        mutable std::mutex PrefixMutex;

        // 最后错误信息
        mutable std::string LastError;
        mutable std::mutex ErrorMutex;

        // 私有方法
        void HotReloadWorker();
        void AutoSaveWorker();
        bool ValidateConfig(const std::string& Key, const ConfigValue& Value, std::string& ErrorMessage);
        std::string ApplyGlobalPrefix(const std::string& Key) const;
        std::string RemoveGlobalPrefix(const std::string& Key) const;
        void UpdateStats(const std::function<void(ConfigStats&)>& Updater) const;
        void SetLastError(const std::string& Error) const;
    };

    // 便捷的全局函数
    namespace GlobalConfig
    {
        // 初始化全局配置
        bool Initialize(const std::string& ConfigFile, ConfigFormat Format = ConfigFormat::AUTO_DETECT, const ConfigManagerConfig& Config = ConfigManagerConfig());

        // 获取配置值
        template<typename T>
        T Get(const std::string& Key, const T& DefaultValue = T{})
        {
            return ConfigManager::Instance().GetValue<T>(Key, DefaultValue);
        }

        // 设置配置值
        bool Set(const std::string& Key, const ConfigValue& Value);

        // 检查配置键是否存在
        bool Has(const std::string& Key);

        // 重新加载配置
        bool Reload();

        // 保存配置
        bool Save();

        // 关闭配置系统
        void Shutdown();
    }

} // namespace Helianthus::Config

// 便捷宏定义
#define HELIANTHUS_CONFIG_GET(key, default_value) \
    Helianthus::Config::GlobalConfig::Get(key, default_value)

#define HELIANTHUS_CONFIG_SET(key, value) \
    Helianthus::Config::GlobalConfig::Set(key, value)

#define HELIANTHUS_CONFIG_HAS(key) \
    Helianthus::Config::GlobalConfig::Has(key)

#define HELIANTHUS_CONFIG_RELOAD() \
    Helianthus::Config::GlobalConfig::Reload()

#define HELIANTHUS_CONFIG_SAVE() \
    Helianthus::Config::GlobalConfig::Save()
