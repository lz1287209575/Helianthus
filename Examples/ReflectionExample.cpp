#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/ReflectionMacros.h"
#include "Shared/Reflection/ScriptBinding.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Common/Logger.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Helianthus::Reflection;
using namespace Helianthus::Scripting;
using namespace Helianthus::Common;

// 示例枚举
enum class PlayerState
{
    Idle,
    Walking,
    Running,
    Jumping,
    Falling
};

// 示例类
class Player
{
public:
    Player() : Name("Unknown"), Health(100), State(PlayerState::Idle) {}
    Player(const std::string& InName) : Name(InName), Health(100), State(PlayerState::Idle) {}
    
    // 属性
    std::string Name;
    int Health;
    PlayerState State;
    
    // 方法
    void SetName(const std::string& NewName) { Name = NewName; }
    std::string GetName() const { return Name; }
    
    void SetHealth(int NewHealth) { Health = std::max(0, std::min(100, NewHealth)); }
    int GetHealth() const { return Health; }
    
    void SetState(PlayerState NewState) { State = NewState; }
    PlayerState GetState() const { return State; }
    
    void TakeDamage(int Damage) 
    { 
        Health = std::max(0, Health - Damage);
        if (Health == 0)
        {
            State = PlayerState::Falling;
        }
    }
    
    void Heal(int Amount) 
    { 
        Health = std::min(100, Health + Amount);
        if (Health > 0 && State == PlayerState::Falling)
        {
            State = PlayerState::Idle;
        }
    }
    
    bool IsAlive() const { return Health > 0; }
    
    void Update(float DeltaTime)
    {
        // 简单的状态更新逻辑
        switch (State)
        {
        case PlayerState::Walking:
            // 走路逻辑
            break;
        case PlayerState::Running:
            // 跑步逻辑
            break;
        case PlayerState::Jumping:
            // 跳跃逻辑
            break;
        case PlayerState::Falling:
            // 坠落逻辑
            break;
        default:
            break;
        }
    }
    
    std::string ToString() const
    {
        return "Player{Name='" + Name + "', Health=" + std::to_string(Health) + 
               ", State=" + std::to_string(static_cast<int>(State)) + "}";
    }
};

// 简化的反射注册（暂时注释掉复杂的宏）
/*
HELIANTHUS_REFLECT_ENUM(PlayerState)
HELIANTHUS_REFLECT_ENUM_VALUE(Idle, PlayerState::Idle)
HELIANTHUS_REFLECT_ENUM_VALUE(Walking, PlayerState::Walking)
HELIANTHUS_REFLECT_ENUM_VALUE(Running, PlayerState::Running)
HELIANTHUS_REFLECT_ENUM_VALUE(Jumping, PlayerState::Jumping)
HELIANTHUS_REFLECT_ENUM_VALUE(Falling, PlayerState::Falling)

HELIANTHUS_REFLECT_CLASS(Player, Object)
HELIANTHUS_REFLECT_PROPERTY(Name, std::string, &Player::Name, &Player::SetName)
HELIANTHUS_REFLECT_PROPERTY(Health, int, &Player::Health, &Player::SetHealth)
HELIANTHUS_REFLECT_PROPERTY(State, PlayerState, &Player::State, &Player::SetState)
HELIANTHUS_REFLECT_METHOD(SetName, void, const std::string&)
HELIANTHUS_REFLECT_METHOD(GetName, std::string)
HELIANTHUS_REFLECT_METHOD(SetHealth, void, int)
HELIANTHUS_REFLECT_METHOD(GetHealth, int)
HELIANTHUS_REFLECT_METHOD(SetState, void, PlayerState)
HELIANTHUS_REFLECT_METHOD(GetState, PlayerState)
HELIANTHUS_REFLECT_METHOD(TakeDamage, void, int)
HELIANTHUS_REFLECT_METHOD(Heal, void, int)
HELIANTHUS_REFLECT_METHOD(IsAlive, bool)
HELIANTHUS_REFLECT_METHOD(Update, void, float)
HELIANTHUS_REFLECT_METHOD(ToString, std::string)

// 自动注册
HELIANTHUS_AUTO_REGISTER_ENUM(PlayerState)
HELIANTHUS_AUTO_REGISTER(Player)
*/

int main()
{
    Logger::Info("Starting Reflection System Example...");
    
    // 初始化反射系统
    InitializeReflectionSystem();
    InitializeScriptBinding();
    
    // 创建脚本引擎
    auto ScriptEngine = std::make_shared<LuaScriptEngine>();
    auto InitResult = ScriptEngine->Initialize();
    if (!InitResult.Success)
    {
        Logger::Error("Failed to initialize script engine: " + InitResult.ErrorMessage);
        return 1;
    }
    
    Logger::Info("=== Basic Reflection System Demo ===");
    
    // 演示反射系统基础功能
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
    
    // 演示脚本引擎功能
    Logger::Info("=== Script Engine Demo ===");
    
    // 执行一个简单的Lua脚本
    std::string LuaScript = R"(
print("Hello from Lua!")
print("Reflection system is working!")

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

print("Lua script loaded successfully!")
)";
    
    auto ScriptResult = ScriptEngine->ExecuteString(LuaScript);
    if (ScriptResult.Success)
    {
        Logger::Info("Successfully executed Lua script");
        
        // 调用Lua函数
        ScriptEngine->CallFunction("greet", {"World"});
    }
    else
    {
        Logger::Error("Failed to execute Lua script: " + ScriptResult.ErrorMessage);
    }
    
    // 演示脚本绑定功能
    Logger::Info("=== Script Binding Demo ===");
    
    // 设置脚本绑定管理器
    if (GlobalScriptBindingManager)
    {
        GlobalScriptBindingManager->SetScriptEngine(ScriptEngine);
        
        // 生成绑定代码
        std::string BindingCode = GlobalScriptBindingManager->GenerateBindingCode("lua");
        Logger::Info("Generated Lua binding code length: " + std::to_string(BindingCode.length()));
        
        // 保存绑定代码到文件
        if (GlobalScriptBindingManager->SaveBindingCode("generated_bindings.lua", "lua"))
        {
            Logger::Info("Saved binding code to generated_bindings.lua");
        }
    }
    
    // 清理
    ShutdownScriptBinding();
    ShutdownReflectionSystem();
    
    Logger::Info("Reflection System Example completed");
    return 0;
}
