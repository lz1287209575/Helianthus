#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>

#include "IConfigProvider.h"

// 需要包含完整的JSON头文件
#include <nlohmann/json.hpp>

namespace Helianthus::Config
{
// JSON配置提供者
class JsonConfigProvider : public IConfigProvider
{
public:
    JsonConfigProvider();
    ~JsonConfigProvider() override;

    // IConfigProvider接口实现
    bool Load(const std::string& Source) override;
    bool Reload() override;
    std::optional<ConfigValue> GetValue(const std::string& Key) const override;
    bool SetValue(const std::string& Key, const ConfigValue& Value) override;
    bool HasKey(const std::string& Key) const override;
    std::vector<std::string> GetAllKeys() const override;
    std::unordered_map<std::string, ConfigValue>
    GetSection(const std::string& Prefix) const override;
    void RegisterChangeCallback(const std::string& Key, ConfigChangeCallback Callback) override;
    void UnregisterChangeCallback(const std::string& Key) override;
    bool Save(const std::string& Destination = "") override;
    std::string GetSource() const override;
    bool IsValid() const override;
    std::string GetLastError() const override;

    // JSON特有功能
    // 从JSON字符串加载配置
    bool LoadFromString(const std::string& JsonString);

    // 获取原始JSON对象（用于高级操作）
    const nlohmann::json& GetRawJson() const;

    // 设置是否启用文件监控（用于热更新）
    void SetFileWatching(bool Enable);

    // 检查文件是否已修改
    bool IsFileModified() const;

private:
    std::unique_ptr<nlohmann::json> JsonData;
    mutable std::mutex DataMutex;
    std::string SourcePath;
    std::string LastError;
    bool IsValidFlag;

    // 文件监控相关
    bool FileWatchingEnabled;
    std::filesystem::file_time_type LastModifiedTime;

    // 配置变更回调
    std::unordered_map<std::string, ConfigChangeCallback> ChangeCallbacks;
    mutable std::mutex CallbackMutex;

    // 内部辅助方法
    ConfigValue JsonValueToConfigValue(const nlohmann::json& JsonValue) const;
    nlohmann::json ConfigValueToJsonValue(const ConfigValue& Value) const;
    void CollectKeysRecursive(const nlohmann::json& JsonObj,
                              const std::string& Prefix,
                              std::vector<std::string>& Keys) const;
    std::optional<nlohmann::json> GetJsonValue(const std::string& Key) const;
    bool SetJsonValue(const std::string& Key, const nlohmann::json& Value);
    void NotifyConfigChange(const std::string& Key,
                            const ConfigValue& OldValue,
                            const ConfigValue& NewValue);
    void UpdateLastModifiedTime();
    std::vector<std::string> SplitKey(const std::string& Key) const;
};

}  // namespace Helianthus::Config
