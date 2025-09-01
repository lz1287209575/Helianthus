-- Generated Lua binding for enum: TestEnum
local TestEnum = {
}
_G.TestEnum = TestEnum
return TestEnum


-- Generated Lua binding for class: TestClass
local TestClass = {}
TestClass.__index = TestClass

function TestClass.new(...)
    local self = setmetatable({}, TestClass)
    -- TODO: Initialize properties
    return self
end

-- Metatable for TestClass
local mt = {
    __index = TestClass,
    __tostring = function(self)
        return "TestClass instance"
    end
}
setmetatable(TestClass, mt)

_G.TestClass = TestClass
return TestClass


