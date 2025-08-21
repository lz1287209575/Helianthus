#include "Shared/Scripting/IScriptEngine.h"
#include "Shared/Scripting/LuaScriptEngine.h"

#include <gtest/gtest.h>
#include <filesystem>

using namespace Helianthus::Scripting;

class ScriptingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Engine = std::make_unique<LuaScriptEngine>();
    }

    void TearDown() override
    {
        Engine.reset();
    }

    std::unique_ptr<IScriptEngine> Engine;
};

TEST_F(ScriptingTest, Initialize)
{
    auto Result = Engine->Initialize();
    ASSERT_TRUE(Result.Success) << "Failed to initialize: " << Result.ErrorMessage;
    ASSERT_EQ(Engine->GetLanguage(), ScriptLanguage::Lua);
}

TEST_F(ScriptingTest, ExecuteString)
{
    ASSERT_TRUE(Engine->Initialize().Success);

    // 测试简单表达式
    auto Result = Engine->ExecuteString("return 1 + 1");
    ASSERT_TRUE(Result.Success) << "Failed to execute: " << Result.ErrorMessage;

    // 测试变量赋值
    Result = Engine->ExecuteString("x = 42");
    ASSERT_TRUE(Result.Success) << "Failed to execute: " << Result.ErrorMessage;

    // 测试错误语法
    Result = Engine->ExecuteString("invalid syntax here");
    ASSERT_FALSE(Result.Success);
    ASSERT_FALSE(Result.ErrorMessage.empty());
}

TEST_F(ScriptingTest, LoadFile)
{
    ASSERT_TRUE(Engine->Initialize().Success);

    // 获取测试脚本路径
    std::filesystem::path ScriptPath = std::filesystem::current_path() / "Scripts" / "hello.lua";
    
    auto Result = Engine->LoadFile(ScriptPath.string());
    ASSERT_TRUE(Result.Success) << "Failed to load file: " << Result.ErrorMessage;

    // 测试加载不存在的文件
    Result = Engine->LoadFile("nonexistent.lua");
    ASSERT_FALSE(Result.Success);
    ASSERT_FALSE(Result.ErrorMessage.empty());
}

TEST_F(ScriptingTest, CallFunction)
{
    ASSERT_TRUE(Engine->Initialize().Success);

    // 先加载脚本
    std::filesystem::path ScriptPath = std::filesystem::current_path() / "Scripts" / "hello.lua";
    ASSERT_TRUE(Engine->LoadFile(ScriptPath.string()).Success);

    // 测试调用 Add 函数
    auto Result = Engine->CallFunction("Add", {"5", "3"});
    ASSERT_TRUE(Result.Success) << "Failed to call Add: " << Result.ErrorMessage;

    // 测试调用 Greet 函数
    Result = Engine->CallFunction("Greet", {"World"});
    ASSERT_TRUE(Result.Success) << "Failed to call Greet: " << Result.ErrorMessage;

    // 测试调用不存在的函数
    Result = Engine->CallFunction("NonexistentFunction", {});
    ASSERT_FALSE(Result.Success);
    ASSERT_FALSE(Result.ErrorMessage.empty());
}

TEST_F(ScriptingTest, ExecuteStringAndCallFunction)
{
    ASSERT_TRUE(Engine->Initialize().Success);

    // 执行字符串定义函数
    auto Result = Engine->ExecuteString(R"(
        function TestFunction(name)
            return "Hello, " .. name .. "!"
        end
    )");
    ASSERT_TRUE(Result.Success) << "Failed to execute: " << Result.ErrorMessage;

    // 调用刚定义的函数
    Result = Engine->CallFunction("TestFunction", {"Alice"});
    ASSERT_TRUE(Result.Success) << "Failed to call TestFunction: " << Result.ErrorMessage;
}

TEST_F(ScriptingTest, ErrorHandling)
{
    ASSERT_TRUE(Engine->Initialize().Success);

    // 测试语法错误
    auto Result = Engine->ExecuteString("print(1 + )");
    ASSERT_FALSE(Result.Success);
    ASSERT_FALSE(Result.ErrorMessage.empty());

    // 测试运行时错误
    Result = Engine->ExecuteString("error('Test error')");
    ASSERT_FALSE(Result.Success);
    ASSERT_FALSE(Result.ErrorMessage.empty());
}

TEST_F(ScriptingTest, MultipleInitializations)
{
    // 第一次初始化
    auto Result = Engine->Initialize();
    ASSERT_TRUE(Result.Success);

    // 第二次初始化应该成功（重新初始化）
    Result = Engine->Initialize();
    ASSERT_TRUE(Result.Success);
}

TEST_F(ScriptingTest, Shutdown)
{
    ASSERT_TRUE(Engine->Initialize().Success);
    
    // 关闭引擎
    Engine->Shutdown();
    
    // 关闭后应该仍然可以重新初始化
    auto Result = Engine->Initialize();
    ASSERT_TRUE(Result.Success);
}
