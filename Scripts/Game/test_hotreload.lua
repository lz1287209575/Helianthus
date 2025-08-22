-- 测试热更新功能的脚本
-- 修改这个文件来测试热更新

local TestModule = {}

TestModule.Version = "1.0.0"
TestModule.Counter = 0

function TestModule.Hello()
    TestModule.Counter = TestModule.Counter + 1
    print(string.format("TestModule: Hello! Counter: %d, Version: %s", TestModule.Counter, TestModule.Version))
    return TestModule.Counter
end

function TestModule.GetInfo()
    return {
        Version = TestModule.Version,
        Counter = TestModule.Counter,
        Message = "This is a test module for hot reload"
    }
end

-- 导出到全局
_G.TestModule = TestModule

print("TestModule: Loaded successfully! Version: " .. TestModule.Version)

return TestModule
