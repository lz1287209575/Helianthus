# Helianthus 配置管理指南

## 概述

Helianthus 配置管理系统提供了一个灵活、强大且易于使用的配置管理解决方案。它支持多种配置源、类型安全、验证、热重载和变更通知等高级功能。

## 主要特性

- **多配置源支持**: 文件、环境变量、命令行参数
- **类型安全**: 支持字符串、整数、浮点数、布尔值、数组、对象
- **配置验证**: 自定义验证器和内置验证
- **热重载**: 配置文件修改时自动重新加载
- **变更通知**: 配置变更时的回调机制
- **配置锁定**: 防止配置被意外修改
- **多格式导出**: JSON、YAML、INI 格式
- **配置模板**: 预定义的配置模板
- **全局访问**: 便捷的全局配置访问接口

## 快速开始

### 1. 基本使用

```cpp
#include "Shared/Config/ConfigManager.h"

using namespace Helianthus::Config;

int main()
{
    // 创建配置管理器
    auto ConfigManager = std::make_unique<ConfigManager>();
    
    // 初始化
    if (!ConfigManager->Initialize("config"))
    {
        return 1;
    }
    
    // 设置配置
    ConfigManager->SetString("app.name", "Helianthus");
    ConfigManager->SetInt("app.port", 8080);
    ConfigManager->SetBool("app.debug", true);
    
    // 获取配置
    std::string AppName = ConfigManager->GetString("app.name");
    int Port = ConfigManager->GetInt("app.port");
    bool Debug = ConfigManager->GetBool("app.debug");
    
    return 0;
}
```

### 2. 全局配置访问

```cpp
#include "Shared/Config/ConfigManager.h"

using namespace Helianthus::Config;

int main()
{
    // 初始化全局配置
    Global::InitializeConfig("config");
    
    // 设置和获取配置
    Global::SetString("app.name", "Helianthus");
    std::string AppName = Global::GetString("app.name");
    
    // 关闭全局配置
    Global::ShutdownConfig();
    
    return 0;
}
```

## 配置源

## 持久化相关配置与调优

### 刷盘策略（I/O 性能）
- `PersistenceConfig.FlushEveryN`（默认 `64`）
  - 累积写入次数达到 N 时触发一次 flush，降低每条消息的 flush 频率以提升吞吐。
- `PersistenceConfig.FlushIntervalMs`（默认 `50`）
  - 自上次 flush 以来的毫秒数超过该值则触发 flush，作为时间上限保证数据落盘的及时性。

调优建议：
- 高吞吐压测：增大 `FlushEveryN` 或 `FlushIntervalMs`（如 `FlushEveryN=256`, `FlushIntervalMs=200`）。
- 低延迟场景：减小两个值以提升持久化及时性（如 `FlushEveryN=16`, `FlushIntervalMs=20`）。
- 结合介质与负载特征（SSD/HDD）观察 `PersistenceStats` 写耗时分布进行调整。

### 1. 配置文件

支持 INI 格式的配置文件：

```ini
# 应用配置
app.name = Helianthus
app.version = 1.0.0
app.port = 8080
app.debug = true

# 消息队列配置
messagequeue.max_size = 10000
messagequeue.enable_batching = true
messagequeue.batch_size = 100
```

加载配置文件：

```cpp
ConfigManager->LoadFromFile("config/helianthus.conf");
```

### 2. 环境变量

环境变量格式：`HELIANTHUS_<配置键>`（大写）

```bash
export HELIANTHUS_APP_NAME="Helianthus"
export HELIANTHUS_APP_PORT="8080"
export HELIANTHUS_APP_DEBUG="true"
```

加载环境变量：

```cpp
ConfigManager->LoadFromEnvironment();
```

### 3. 命令行参数

支持 `--key=value` 或 `--key value` 格式：

```bash
./helianthus --app.name=Helianthus --app.port=8080 --app.debug=true
```

加载命令行参数：

```cpp
ConfigManager->LoadFromCommandLine(argc, argv);
```

## 配置类型

### 1. 基本类型

```cpp
// 字符串
ConfigManager->SetString("app.name", "Helianthus");
std::string Name = ConfigManager->GetString("app.name", "default");

// 整数
ConfigManager->SetInt("app.port", 8080);
int64_t Port = ConfigManager->GetInt("app.port", 8080);

// 浮点数
ConfigManager->SetFloat("app.version", 1.0);
double Version = ConfigManager->GetFloat("app.version", 1.0);

// 布尔值
ConfigManager->SetBool("app.debug", true);
bool Debug = ConfigManager->GetBool("app.debug", false);
```

### 2. 复杂类型

```cpp
// 数组
std::vector<std::string> Nodes = {"node1", "node2", "node3"};
ConfigManager->SetArray("cluster.nodes", Nodes);
auto RetrievedNodes = ConfigManager->GetArray("cluster.nodes");

// 对象
std::unordered_map<std::string, std::string> Config = {
    {"key1", "value1"},
    {"key2", "value2"}
};
ConfigManager->SetObject("app.config", Config);
auto RetrievedConfig = ConfigManager->GetObject("app.config");
```

### 3. 类型转换

配置值支持自动类型转换：

```cpp
// 字符串转数字
ConfigManager->SetString("port", "8080");
int64_t Port = ConfigManager->GetInt("port"); // 8080

// 数字转字符串
ConfigManager->SetInt("version", 1);
std::string Version = ConfigManager->GetString("version"); // "1"

// 字符串转布尔值
ConfigManager->SetString("debug", "true");
bool Debug = ConfigManager->GetBool("debug"); // true
```

## 配置验证

### 1. 内置验证

```cpp
// 空字符串验证
ConfigValue EmptyString("");
bool IsValid = EmptyString.IsValid(); // false

// 数字验证
ConfigValue ValidInt(42);
bool IsValid = ValidInt.IsValid(); // true

// 浮点数验证
ConfigValue ValidFloat(3.14);
bool IsValid = ValidFloat.IsValid(); // true
```

### 2. 自定义验证器

```cpp
// 端口验证器
ConfigValidator PortValidator = [](const std::string& Key, const ConfigValue& Value) {
    int64_t Port = Value.AsInt();
    return Port > 0 && Port <= 65535;
};

ConfigManager->AddValidator("app.port", PortValidator);

// 使用验证器
if (ConfigManager->SetInt("app.port", 8080)) {
    // 验证通过
} else {
    // 验证失败
}
```

### 3. 配置验证

```cpp
// 验证单个配置项
bool IsValid = ConfigManager->ValidateConfigItem("app.port");

// 验证所有配置
bool AllValid = ConfigManager->ValidateConfig();
```

## 配置变更通知

### 1. 单个配置变更回调

```cpp
ConfigChangeCallback Callback = [](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue) {
    std::cout << "配置变更: " << Key << " = " << OldValue.ToString() << " -> " << NewValue.ToString() << std::endl;
};

ConfigManager->AddChangeCallback("app.name", Callback);

// 触发变更
ConfigManager->SetString("app.name", "NewName");
```

### 2. 全局配置变更回调

```cpp
ConfigChangeCallback GlobalCallback = [](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue) {
    std::cout << "全局配置变更: " << Key << std::endl;
};

ConfigManager->AddGlobalChangeCallback(GlobalCallback);
```

## 配置锁定

```cpp
// 锁定配置
ConfigManager->LockConfig();

// 尝试修改配置（将失败）
bool Success = ConfigManager->SetString("app.name", "NewName"); // false

// 解锁配置
ConfigManager->UnlockConfig();

// 现在可以修改配置
Success = ConfigManager->SetString("app.name", "NewName"); // true
```

## 热重载

```cpp
// 启用热重载
ConfigManager->EnableHotReload(true);

// 配置文件修改时会自动重新加载
// 可以通过回调函数处理配置变更

// 禁用热重载
ConfigManager->EnableHotReload(false);
```

## 配置导出

### 1. JSON 格式

```cpp
std::string JsonConfig = ConfigManager->ExportToJson();
std::cout << JsonConfig << std::endl;
```

输出示例：
```json
{
  "app.name": "Helianthus",
  "app.port": 8080,
  "app.debug": true,
  "messagequeue.max_size": 10000
}
```

### 2. YAML 格式

```cpp
std::string YamlConfig = ConfigManager->ExportToYaml();
std::cout << YamlConfig << std::endl;
```

输出示例：
```yaml
app.name: Helianthus
app.port: 8080
app.debug: true
messagequeue.max_size: 10000
```

### 3. INI 格式

```cpp
std::string IniConfig = ConfigManager->ExportToIni();
std::cout << IniConfig << std::endl;
```

输出示例：
```ini
app.name = Helianthus
app.port = 8080
app.debug = true
messagequeue.max_size = 10000
```

## 配置模板

### 1. 预定义模板

```cpp
// 加载消息队列配置模板
ConfigTemplate::LoadMessageQueueDefaults(*ConfigManager);

// 加载网络配置模板
ConfigTemplate::LoadNetworkDefaults(*ConfigManager);

// 加载日志配置模板
ConfigTemplate::LoadLoggingDefaults(*ConfigManager);

// 加载监控配置模板
ConfigTemplate::LoadMonitoringDefaults(*ConfigManager);

// 加载安全配置模板
ConfigTemplate::LoadSecurityDefaults(*ConfigManager);

// 加载性能配置模板
ConfigTemplate::LoadPerformanceDefaults(*ConfigManager);
```

### 2. 自定义模板

```cpp
void LoadCustomDefaults(ConfigManager& Manager)
{
    Manager.SetString("custom.key1", "value1");
    Manager.SetInt("custom.key2", 42);
    Manager.SetBool("custom.key3", true);
}

// 使用自定义模板
LoadCustomDefaults(*ConfigManager);
```

## 配置项管理

### 1. 添加配置项

```cpp
ConfigItem Item("app.name", ConfigValue("Helianthus"), "Application name");
ConfigManager->AddConfigItem(Item);
```

### 2. 获取配置项

```cpp
ConfigItem Item = ConfigManager->GetConfigItem("app.name");
std::cout << "Key: " << Item.Key << std::endl;
std::cout << "Value: " << Item.Value.ToString() << std::endl;
std::cout << "Description: " << Item.Description << std::endl;
```

### 3. 检查配置项

```cpp
if (ConfigManager->HasConfigItem("app.name")) {
    // 配置项存在
}
```

### 4. 删除配置项

```cpp
ConfigManager->RemoveConfigItem("app.name");
```

### 5. 获取所有配置键

```cpp
auto Keys = ConfigManager->GetAllKeys();
for (const auto& Key : Keys) {
    std::cout << "Key: " << Key << std::endl;
}
```

## 修改跟踪

### 1. 获取修改的配置项

```cpp
auto ModifiedKeys = ConfigManager->GetModifiedKeys();
for (const auto& Key : ModifiedKeys) {
    std::cout << "Modified: " << Key << std::endl;
}
```

### 2. 清除修改标志

```cpp
ConfigManager->ClearModifiedFlags();
```

## 配置统计

```cpp
// 获取配置项数量
size_t Count = ConfigManager->GetConfigItemCount();

// 获取修改的配置项数量
auto ModifiedKeys = ConfigManager->GetModifiedKeys();
size_t ModifiedCount = ModifiedKeys.size();

// 检查配置验证状态
bool IsValid = ConfigManager->ValidateConfig();
```

## 最佳实践

### 1. 配置键命名

- 使用小写字母和点号分隔
- 使用有意义的层次结构
- 避免特殊字符

```cpp
// 好的命名
app.name
messagequeue.max_size
network.connection_timeout_ms

// 避免的命名
APP_NAME
messageQueue.maxSize
network.connectionTimeout
```

### 2. 配置验证

- 为关键配置添加验证器
- 验证配置值的范围和格式
- 提供有意义的错误信息

```cpp
// 端口验证器
ConfigValidator PortValidator = [](const std::string& Key, const ConfigValue& Value) {
    int64_t Port = Value.AsInt();
    if (Port <= 0 || Port > 65535) {
        std::cerr << "Invalid port: " << Port << std::endl;
        return false;
    }
    return true;
};
```

### 3. 配置变更处理

- 使用回调函数处理配置变更
- 避免在回调函数中修改配置
- 记录配置变更日志

```cpp
ConfigChangeCallback LoggingCallback = [](const std::string& Key, const ConfigValue& OldValue, const ConfigValue& NewValue) {
    Logger::Info("Config changed: {} = {} -> {}", Key, OldValue.ToString(), NewValue.ToString());
};
```

### 4. 配置安全

- 在生产环境中锁定关键配置
- 验证配置文件的权限
- 使用环境变量存储敏感信息

```cpp
// 生产环境配置锁定
if (ConfigManager->GetString("app.environment") == "production") {
    ConfigManager->LockConfig();
}
```

### 5. 配置文档

- 为配置项添加描述
- 提供配置示例
- 记录配置变更历史

```cpp
ConfigItem Item("app.port", ConfigValue(8080), "Application server port (1-65535)");
ConfigManager->AddConfigItem(Item);
```

## 故障排除

### 1. 常见问题

**配置加载失败**
- 检查文件路径是否正确
- 验证文件权限
- 检查配置文件格式

**配置验证失败**
- 检查配置值类型
- 验证自定义验证器逻辑
- 查看错误日志

**热重载不工作**
- 确认已启用热重载
- 检查文件监控权限
- 验证文件修改时间

### 2. 调试技巧

```cpp
// 启用调试日志
ConfigManager->SetBool("app.debug", true);

// 检查配置状态
std::cout << "Config count: " << ConfigManager->GetConfigItemCount() << std::endl;
std::cout << "Is initialized: " << ConfigManager->IsInitialized() << std::endl;
std::cout << "Is locked: " << ConfigManager->IsConfigLocked() << std::endl;

// 导出配置进行调试
std::string DebugConfig = ConfigManager->ExportToJson();
std::cout << "Current config: " << DebugConfig << std::endl;
```

## API 参考

### ConfigManager 类

#### 构造函数
```cpp
ConfigManager();
```

#### 初始化方法
```cpp
bool Initialize(const std::string& ConfigPath = "config/");
void Shutdown();
bool IsInitialized() const;
```

#### 配置加载方法
```cpp
bool LoadFromFile(const std::string& FilePath);
bool LoadFromEnvironment();
bool LoadFromCommandLine(int Argc, char* Argv[]);
bool ReloadConfig();
```

#### 配置设置方法
```cpp
bool SetValue(const std::string& Key, const ConfigValue& Value);
bool SetString(const std::string& Key, const std::string& Value);
bool SetInt(const std::string& Key, int64_t Value);
bool SetFloat(const std::string& Key, double Value);
bool SetBool(const std::string& Key, bool Value);
bool SetArray(const std::string& Key, const std::vector<std::string>& Value);
bool SetObject(const std::string& Key, const std::unordered_map<std::string, std::string>& Value);
```

#### 配置获取方法
```cpp
ConfigValue GetValue(const std::string& Key) const;
std::string GetString(const std::string& Key, const std::string& Default = "") const;
int64_t GetInt(const std::string& Key, int64_t Default = 0) const;
double GetFloat(const std::string& Key, double Default = 0.0) const;
bool GetBool(const std::string& Key, bool Default = false) const;
std::vector<std::string> GetArray(const std::string& Key) const;
std::unordered_map<std::string, std::string> GetObject(const std::string& Key) const;
```

#### 配置管理方法
```cpp
bool AddConfigItem(const ConfigItem& Item);
bool RemoveConfigItem(const std::string& Key);
bool HasConfigItem(const std::string& Key) const;
ConfigItem GetConfigItem(const std::string& Key) const;
std::vector<std::string> GetAllKeys() const;
```

#### 验证方法
```cpp
bool ValidateConfig() const;
bool ValidateConfigItem(const std::string& Key) const;
void AddValidator(const std::string& Key, ConfigValidator Validator);
void RemoveValidator(const std::string& Key);
```

#### 变更通知方法
```cpp
void AddChangeCallback(const std::string& Key, ConfigChangeCallback Callback);
void RemoveChangeCallback(const std::string& Key);
void AddGlobalChangeCallback(ConfigChangeCallback Callback);
void RemoveGlobalChangeCallback();
```

#### 热重载方法
```cpp
bool EnableHotReload(bool Enable = true);
bool IsHotReloadEnabled() const;
```

#### 配置模板方法
```cpp
void LoadDefaultConfig();
void LoadMessageQueueConfig();
void LoadNetworkConfig();
void LoadLoggingConfig();
void LoadMonitoringConfig();
```

#### 导出方法
```cpp
std::string ExportToJson() const;
std::string ExportToYaml() const;
std::string ExportToIni() const;
```

#### 导入方法
```cpp
bool ImportFromJson(const std::string& JsonData);
bool ImportFromYaml(const std::string& YamlData);
bool ImportFromIni(const std::string& IniData);
```

#### 统计方法
```cpp
size_t GetConfigItemCount() const;
std::vector<std::string> GetModifiedKeys() const;
void ClearModifiedFlags();
```

#### 锁定方法
```cpp
void LockConfig();
void UnlockConfig();
bool IsConfigLocked() const;
```

### Global 命名空间

#### 初始化方法
```cpp
bool InitializeConfig(const std::string& ConfigPath = "config/");
void ShutdownConfig();
```

#### 配置访问方法
```cpp
std::string GetString(const std::string& Key, const std::string& Default = "");
int64_t GetInt(const std::string& Key, int64_t Default = 0);
double GetFloat(const std::string& Key, double Default = 0.0);
bool GetBool(const std::string& Key, bool Default = false);
```

#### 配置设置方法
```cpp
bool SetString(const std::string& Key, const std::string& Value);
bool SetInt(const std::string& Key, int64_t Value);
bool SetFloat(const std::string& Key, double Value);
bool SetBool(const std::string& Key, bool Value);
```

#### 验证方法
```cpp
bool ValidateConfig();
bool ReloadConfig();
```

### ConfigTemplate 类

#### 静态方法
```cpp
static void LoadMessageQueueDefaults(ConfigManager& Manager);
static void LoadNetworkDefaults(ConfigManager& Manager);
static void LoadLoggingDefaults(ConfigManager& Manager);
static void LoadMonitoringDefaults(ConfigManager& Manager);
static void LoadSecurityDefaults(ConfigManager& Manager);
static void LoadPerformanceDefaults(ConfigManager& Manager);
```

## 示例代码

完整的示例代码请参考 `Examples/ConfigExample.cpp`。

## 总结

Helianthus 配置管理系统提供了完整的配置管理解决方案，支持多种配置源、类型安全、验证、热重载等高级功能。通过合理使用这些功能，可以构建灵活、可维护的应用程序配置系统。
