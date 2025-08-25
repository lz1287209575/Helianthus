#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>
#include <optional>

namespace Helianthus::Config
{
    // 配置值类型
    using ConfigValue = std::variant<
        std::string,
        int32_t,
        int64_t,
        uint32_t,
        uint64_t,
        double,
        bool
    >;

    // 配置变更回调类型
    using ConfigChangeCallback = std::function<void(const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue)>;

    // 配置提供者接口
    class IConfigProvider
    {
    public:
        virtual ~IConfigProvider() = default;

        // 加载配置
        virtual bool Load(const std::string& Source) = 0;

        // 重新加载配置
        virtual bool Reload() = 0;

        // 获取配置值
        virtual std::optional<ConfigValue> GetValue(const std::string& Key) const = 0;

        // 获取配置值（带默认值）
        template<typename T>
        T GetValue(const std::string& Key, const T& DefaultValue) const
        {
            auto Value = GetValue(Key);
            if (!Value.has_value())
            {
                return DefaultValue;
            }

            try
            {
                return std::get<T>(*Value);
            }
            catch (const std::bad_variant_access&)
            {
                return DefaultValue;
            }
        }

        // 设置配置值
        virtual bool SetValue(const std::string& Key, const ConfigValue& Value) = 0;

        // 检查配置键是否存在
        virtual bool HasKey(const std::string& Key) const = 0;

        // 获取所有配置键
        virtual std::vector<std::string> GetAllKeys() const = 0;

        // 获取配置节（以前缀匹配）
        virtual std::unordered_map<std::string, ConfigValue> GetSection(const std::string& Prefix) const = 0;

        // 注册配置变更回调
        virtual void RegisterChangeCallback(const std::string& Key, ConfigChangeCallback Callback) = 0;

        // 注销配置变更回调
        virtual void UnregisterChangeCallback(const std::string& Key) = 0;

        // 保存配置到文件
        virtual bool Save(const std::string& Destination = "") = 0;

        // 获取配置源信息
        virtual std::string GetSource() const = 0;

        // 检查配置是否有效
        virtual bool IsValid() const = 0;

        // 获取最后错误信息
        virtual std::string GetLastError() const = 0;
    };

    // 配置格式类型
    enum class ConfigFormat
    {
        JSON,
        YAML,
        AUTO_DETECT  // 根据文件扩展名自动检测
    };

    // 配置工厂
    class ConfigFactory
    {
    public:
        // 创建配置提供者
        static std::unique_ptr<IConfigProvider> CreateProvider(ConfigFormat Format);

        // 根据文件路径自动创建配置提供者
        static std::unique_ptr<IConfigProvider> CreateProviderFromFile(const std::string& FilePath);
    };

} // namespace Helianthus::Config
