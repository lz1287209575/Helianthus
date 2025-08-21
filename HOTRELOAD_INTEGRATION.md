# HotReloadManager 集成指南

## 概述

`HotReloadManager` 是 Helianthus 框架中的脚本热更新管理器，支持在运行时监控脚本文件变更并自动重新加载，无需重启应用程序。

## 功能特性

- **文件监控**: 支持监控多个目录和文件扩展名
- **自动重载**: 检测到文件变更时自动重新加载脚本
- **回调通知**: 提供重载成功/失败的回调通知
- **线程安全**: 支持多线程环境下的安全操作
- **可配置**: 支持自定义轮询间隔、文件扩展名等

## 集成步骤

### 1. 包含头文件

```cpp
#include "Shared/Scripting/IScriptEngine.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Scripting/HotReloadManager.h"
```

### 2. 初始化脚本引擎

```cpp
// 创建并初始化 Lua 脚本引擎
auto ScriptEngine = std::make_shared<LuaScriptEngine>();
auto InitResult = ScriptEngine->Initialize();
if (!InitResult.Success) {
    // 处理初始化失败
    return;
}
```

### 3. 配置热更新管理器

```cpp
// 创建热更新管理器
auto HotReload = std::make_unique<HotReloadManager>();

// 设置脚本引擎
HotReload->SetEngine(ScriptEngine);

// 配置轮询间隔（毫秒）
HotReload->SetPollIntervalMs(1000);

// 设置要监控的文件扩展名
HotReload->SetFileExtensions({".lua", ".py", ".js"});

// 设置重载回调
HotReload->SetOnFileReloaded([](const std::string& ScriptPath, bool Success, const std::string& ErrorMessage) {
    if (Success) {
        std::cout << "Script reloaded: " << ScriptPath << std::endl;
    } else {
        std::cerr << "Script reload failed: " << ScriptPath << " - " << ErrorMessage << std::endl;
    }
});
```

### 4. 添加监控路径

```cpp
// 添加要监控的目录
HotReload->AddWatchPath("Scripts");
HotReload->AddWatchPath("Scripts/Game");
HotReload->AddWatchPath("Config");
```

### 5. 启动热更新

```cpp
// 启动热更新监控
HotReload->Start();
```

### 6. 在应用关闭时停止

```cpp
// 停止热更新监控
HotReload->Stop();
```

## 完整示例

参考 `Examples/GameServerWithHotReload.cpp` 查看完整的集成示例。

## 脚本编写指南

### Lua 脚本示例

```lua
-- 游戏逻辑脚本示例
local GameLogic = {}

GameLogic.PlayerCount = 0
GameLogic.GameTime = 0

function GameLogic.Initialize()
    print("GameLogic: Initializing...")
    GameLogic.PlayerCount = 0
    GameLogic.GameTime = 0
end

function GameLogic.Update(DeltaTime)
    GameLogic.GameTime = GameLogic.GameTime + DeltaTime
    print("GameLogic: Update called, time: " .. GameLogic.GameTime)
end

-- 导出到全局
_G.GameLogic = GameLogic

print("GameLogic: Script loaded successfully!")
return GameLogic
```

### 热更新最佳实践

1. **状态保持**: 在脚本重载时保持重要的游戏状态
2. **错误处理**: 在回调中处理重载失败的情况
3. **版本管理**: 在脚本中包含版本信息便于调试
4. **模块化**: 将不同功能的脚本分离到不同文件

## 配置选项

### 轮询间隔

```cpp
// 设置轮询间隔为 500 毫秒（更快的响应）
HotReload->SetPollIntervalMs(500);

// 设置轮询间隔为 2000 毫秒（更低的 CPU 使用）
HotReload->SetPollIntervalMs(2000);
```

### 文件扩展名

```cpp
// 只监控 Lua 文件
HotReload->SetFileExtensions({".lua"});

// 监控多种脚本文件
HotReload->SetFileExtensions({".lua", ".py", ".js", ".ts"});
```

### 监控路径

```cpp
// 监控多个目录
HotReload->AddWatchPath("Scripts");
HotReload->AddWatchPath("Scripts/Game");
HotReload->AddWatchPath("Scripts/UI");
HotReload->AddWatchPath("Config");

// 清除所有监控路径
HotReload->ClearWatchPaths();
```

## 构建配置

### 启用 Lua 支持

```bash
# 构建时启用 Lua 脚本支持
bazel build //Examples:game_server_with_hotreload --define ENABLE_LUA_SCRIPTING=1

# 运行测试
bazel test //Tests/Scripting:scripting_test --define ENABLE_LUA_SCRIPTING=1
```

### 依赖项

确保在 `BUILD.bazel` 文件中包含必要的依赖：

```python
cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    deps = [
        "//Shared/Scripting:scripting",
        "//Shared/Network:network",
        "//Shared/Common:common",
    ],
)
```

## 故障排除

### 常见问题

1. **脚本重载失败**: 检查脚本语法错误和路径是否正确
2. **文件监控不工作**: 确认文件路径存在且有读取权限
3. **性能问题**: 调整轮询间隔或减少监控路径数量
4. **编译错误**: 确保启用了 `ENABLE_LUA_SCRIPTING` 定义

### 调试技巧

1. 在重载回调中添加详细的日志输出
2. 使用 `IsFileLoaded()` 检查文件是否已加载
3. 使用 `GetLoadedFiles()` 获取所有已加载的文件列表
4. 检查脚本引擎的初始化状态

## 扩展功能

### 自定义重载策略

```cpp
HotReload->SetOnFileReloaded([](const std::string& ScriptPath, bool Success, const std::string& ErrorMessage) {
    if (Success) {
        // 根据文件类型执行不同的重载逻辑
        if (ScriptPath.find("game_logic.lua") != std::string::npos) {
            // 重新初始化游戏逻辑
            ScriptEngine->CallFunction("GameLogic.Initialize", {});
        } else if (ScriptPath.find("ui.lua") != std::string::npos) {
            // 重新加载 UI 配置
            ScriptEngine->CallFunction("UI.Reload", {});
        }
    }
});
```

### 条件重载

```cpp
// 只在特定条件下重载脚本
if (IsDevelopmentMode()) {
    HotReload->Start();
}
```

## 性能考虑

- **轮询间隔**: 较低的轮询间隔提供更快的响应，但会增加 CPU 使用
- **监控路径**: 减少监控路径数量可以提高性能
- **文件大小**: 大文件的变更检测可能需要更多时间
- **并发访问**: 确保脚本引擎支持并发访问

## 安全注意事项

- 只监控可信的脚本目录
- 验证脚本内容的安全性
- 限制脚本的执行权限
- 在生产环境中谨慎使用热更新功能
