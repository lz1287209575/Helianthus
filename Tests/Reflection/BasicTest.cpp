#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Common/Logger.h"

#include <memory>
#include <string>
#include <iostream>

using namespace Helianthus::Scripting;
using namespace Helianthus::Common;

int main()
{
    Logger::Info("Starting Basic Script Engine Test...");
    
    // 测试脚本引擎功能
    Logger::Info("=== Script Engine Test ===");
    
    auto ScriptEngine = std::make_shared<LuaScriptEngine>();
    auto InitResult = ScriptEngine->Initialize();
    if (InitResult.Success)
    {
        Logger::Info("Script engine initialized successfully");
        
        // 执行一个简单的Lua脚本
        std::string LuaScript = R"(
print("Hello from Lua in basic test!")
print("Script engine is working!")

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
    
    Logger::Info("Basic Script Engine Test completed");
    return 0;
}
