#include "Shared/Common/Logger.h"
#include "Shared/Scripting/HotReloadManager.h"
#include "Shared/Scripting/IScriptEngine.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Scripting/PythonScriptEngine.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace Helianthus::Scripting;
using namespace Helianthus::Common;

int main()
{
    Logger::Info("Starting Multi-Language Scripting Example...");

    // 创建Lua脚本引擎
    auto LuaEngine = std::make_shared<LuaScriptEngine>();
    auto LuaInit = LuaEngine->Initialize();
    if (!LuaInit.Success)
    {
        Logger::Error("Failed to initialize Lua engine: " + LuaInit.ErrorMessage);
        return 1;
    }
    Logger::Info("Lua script engine initialized successfully");

    // 创建Python脚本引擎
    auto PythonEngine = std::make_shared<PythonScriptEngine>();
    auto PythonInit = PythonEngine->Initialize();
    if (!PythonInit.Success)
    {
        Logger::Warn("Failed to initialize Python engine: " + PythonInit.ErrorMessage);
        Logger::Info("Continuing with Lua only...");
    }
    else
    {
        Logger::Info("Python script engine initialized successfully");
    }

    // 演示Lua脚本
    Logger::Info("=== Lua Scripting Demo ===");

    std::string LuaCode = R"(
-- Lua脚本示例
print("Hello from Lua!")

-- 定义一个简单的函数
function greet(name)
    return "Hello, " .. name .. " from Lua!"
end

-- 定义一个计算函数
function calculate(a, b, operation)
    if operation == "add" then
        return a + b
    elseif operation == "subtract" then
        return a - b
    elseif operation == "multiply" then
        return a * b
    elseif operation == "divide" then
        if b ~= 0 then
            return a / b
        else
            error("Division by zero")
        end
    else
        error("Unknown operation: " .. tostring(operation))
    end
end

-- 定义一个简单的类
local Calculator = {}
Calculator.__index = Calculator

function Calculator.new()
    local self = setmetatable({}, Calculator)
    self.history = {}
    return self
end

function Calculator:add(a, b)
    local result = a + b
    table.insert(self.history, string.format("%d + %d = %d", a, b, result))
    return result
end

function Calculator:getHistory()
    return self.history
end

-- 导出到全局
_G.Calculator = Calculator

print("Lua script loaded successfully!")
)";

    auto LuaResult = LuaEngine->ExecuteString(LuaCode);
    if (LuaResult.Success)
    {
        Logger::Info("Lua script executed successfully");

        // 调用Lua函数
        LuaEngine->CallFunction("greet", {"World"});

        // 测试计算器
        auto Calc = LuaEngine->ExecuteString("local calc = Calculator.new()");
        if (Calc.Success)
        {
            Logger::Info("Calculator created in Lua");
        }
    }
    else
    {
        Logger::Error("Lua script execution failed: " + LuaResult.ErrorMessage);
    }

    // 演示Python脚本（如果可用）
    if (PythonInit.Success)
    {
        Logger::Info("=== Python Scripting Demo ===");

        std::string PythonCode = R"(
# Python脚本示例
print("Hello from Python!")

# 定义一个简单的函数
def greet(name):
    return f"Hello, {name} from Python!"

# 定义一个计算函数
def calculate(a, b, operation):
    if operation == "add":
        return a + b
    elif operation == "subtract":
        return a - b
    elif operation == "multiply":
        return a * b
    elif operation == "divide":
        if b != 0:
            return a / b
        else:
            raise ValueError("Division by zero")
    else:
        raise ValueError(f"Unknown operation: {operation}")

# 定义一个简单的类
class Calculator:
    def __init__(self):
        self.history = []
    
    def add(self, a, b):
        result = a + b
        self.history.append(f"{a} + {b} = {result}")
        return result
    
    def get_history(self):
        return self.history

print("Python script loaded successfully!")
)";

        auto PythonResult = PythonEngine->ExecuteString(PythonCode);
        if (PythonResult.Success)
        {
            Logger::Info("Python script executed successfully");

            // 调用Python函数
            PythonEngine->CallFunction("greet", {"World"});

            // 测试计算器
            auto Calc = PythonEngine->ExecuteString("calc = Calculator()");
            if (Calc.Success)
            {
                Logger::Info("Calculator created in Python");
            }
        }
        else
        {
            Logger::Error("Python script execution failed: " + PythonResult.ErrorMessage);
        }
    }

    // 演示热更新功能
    Logger::Info("=== Hot Reload Demo ===");

    // 创建热更新管理器（使用Lua引擎）
    HotReloadManager HotReload;
    HotReload.SetEngine(LuaEngine);
    HotReload.SetPollIntervalMs(1000);
    HotReload.SetFileExtensions({".lua"});
    HotReload.SetOnFileReloaded(
        [](const std::string& ScriptPath, bool Success, const std::string& ErrorMessage)
        {
            if (Success)
            {
                Logger::Info("Script reloaded successfully: " + ScriptPath);
            }
            else
            {
                Logger::Error("Script reload failed: " + ScriptPath + " - " + ErrorMessage);
            }
        });

    // 添加监控路径
    HotReload.AddWatchPath("Scripts");
    HotReload.Start();

    Logger::Info("Hot reload manager started");
    Logger::Info("Monitoring Scripts/ directory for .lua file changes");
    Logger::Info("Press Ctrl+C to exit");

    // 运行一段时间
    try
    {
        for (int i = 0; i < 30; ++i)  // 运行30秒
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // 每10秒输出一次状态
            if (i % 10 == 0)
            {
                Logger::Info("Multi-language scripting example running... (" + std::to_string(i) +
                             "/30 seconds)");
            }
        }
    }
    catch (const std::exception& E)
    {
        Logger::Error("Exception in main loop: " + std::string(E.what()));
    }

    // 停止热更新
    HotReload.Stop();

    Logger::Info("Multi-Language Scripting Example completed");
    return 0;
}
