#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/ScriptBinding.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Common/Logger.h"

#include <memory>
#include <string>
#include <iostream>

using namespace Helianthus::Reflection;
using namespace Helianthus::Scripting;
using namespace Helianthus::Common;

int main()
{
    Logger::Info("Starting Simple Reflection System Test...");
    
    // 初始化反射系统
    InitializeReflectionSystem();
    InitializeScriptBinding();
    
    Logger::Info("=== Basic Reflection System Test ===");
    
    // 测试反射系统基础功能
    if (GlobalReflectionSystem)
    {
        Logger::Info("Reflection system initialized successfully");
        
        // 列出所有注册的类
        auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
        Logger::Info("Registered classes: " + std::to_string(ClassNames.size()));
        
        // 列出所有注册的枚举
        auto EnumNames = GlobalReflectionSystem->GetAllEnumNames();
        Logger::Info("Registered enums: " + std::to_string(EnumNames.size()));
    }
    else
    {
        Logger::Error("Failed to initialize reflection system");
    }
    
    // 测试脚本引擎功能
    Logger::Info("=== Script Engine Test ===");
    
    auto ScriptEngine = std::make_shared<LuaScriptEngine>();
    auto InitResult = ScriptEngine->Initialize();
    if (InitResult.Success)
    {
        Logger::Info("Script engine initialized successfully");
        
        // 执行一个简单的Lua脚本
        std::string LuaScript = R"(
print("Hello from Lua in reflection test!")
print("Reflection system is working!")

-- 定义一个简单的函数
function test_function()
    return "Test function called successfully!"
end

print("Lua script loaded successfully!")
)";
        
        auto ScriptResult = ScriptEngine->ExecuteString(LuaScript);
        if (ScriptResult.Success)
        {
            Logger::Info("Successfully executed Lua script");
            
            // 调用Lua函数
            ScriptEngine->CallFunction("test_function", {});
        }
        else
        {
            Logger::Error("Failed to execute Lua script: " + ScriptResult.ErrorMessage);
        }
    }
    else
    {
        Logger::Error("Failed to initialize script engine: " + InitResult.ErrorMessage);
    }
    
    // 测试脚本绑定功能
    Logger::Info("=== Script Binding Test ===");
    
    if (GlobalScriptBindingManager)
    {
        Logger::Info("Script binding manager initialized successfully");
        
        // 设置脚本引擎
        GlobalScriptBindingManager->SetScriptEngine(ScriptEngine);
        
        // 生成绑定代码
        std::string BindingCode = GlobalScriptBindingManager->GenerateBindingCode("lua");
        Logger::Info("Generated Lua binding code length: " + std::to_string(BindingCode.length()));
        
        // 保存绑定代码到文件
        if (GlobalScriptBindingManager->SaveBindingCode("test_bindings.lua", "lua"))
        {
            Logger::Info("Saved binding code to test_bindings.lua");
        }
        else
        {
            Logger::Warn("Failed to save binding code");
        }
    }
    else
    {
        Logger::Error("Script binding manager is null");
    }
    
    // 清理
    ShutdownScriptBinding();
    ShutdownReflectionSystem();
    
    Logger::Info("Simple Reflection System Test completed");
    return 0;
}
