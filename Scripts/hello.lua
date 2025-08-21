-- 简单的 Lua 示例脚本
print("Hello from Lua!")

-- 定义一个简单的函数
function Add(a, b)
    return a + b
end

-- 定义一个字符串处理函数
function Greet(name)
    return "Hello, " .. name .. "!"
end

-- 定义一个返回多个值的函数
function GetInfo()
    return "Lua", 5.4, true
end

-- 定义一个带默认参数的函数
function SayHello(name, greeting)
    greeting = greeting or "Hello"
    return greeting .. ", " .. name .. "!"
end

-- 定义一个简单的计算函数
function Calculate(x, y, operation)
    if operation == "add" then
        return x + y
    elseif operation == "subtract" then
        return x - y
    elseif operation == "multiply" then
        return x * y
    elseif operation == "divide" then
        if y ~= 0 then
            return x / y
        else
            error("Division by zero")
        end
    else
        error("Unknown operation: " .. tostring(operation))
    end
end

print("Script loaded successfully!")
