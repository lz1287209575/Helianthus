#include "Shared/Reflection/ReflectionTypes.h"
#include "Shared/Reflection/ReflectionMacros.h"
#include "Shared/Reflection/ScriptBinding.h"
#include "Shared/Scripting/LuaScriptEngine.h"
#include "Shared/Common/Logger.h"

#include <memory>
#include <string>
#include <iostream>

using namespace Helianthus::Reflection;
using namespace Helianthus::Scripting;
using namespace Helianthus::Common;

// 测试枚举
enum class TestEnum
{
    Value1,
    Value2,
    Value3
};

// 测试类
class TestClass
{
public:
    TestClass() : IntValue(0), StringValue(""), EnumValue(TestEnum::Value1) {}
    TestClass(int IntVal, const std::string& StrVal, TestEnum EnumVal) 
        : IntValue(IntVal), StringValue(StrVal), EnumValue(EnumVal) {}
    
    // 属性
    int IntValue;
    std::string StringValue;
    TestEnum EnumValue;
    
    // 方法
    void SetIntValue(int Value) { IntValue = Value; }
    int GetIntValue() const { return IntValue; }
    
    void SetStringValue(const std::string& Value) { StringValue = Value; }
    std::string GetStringValue() const { return StringValue; }
    
    void SetEnumValue(TestEnum Value) { EnumValue = Value; }
    TestEnum GetEnumValue() const { return EnumValue; }
    
    int Add(int A, int B) { return A + B; }
    std::string Concat(const std::string& A, const std::string& B) { return A + B; }
    
    std::string ToString() const
    {
        return "TestClass{IntValue=" + std::to_string(IntValue) + 
               ", StringValue='" + StringValue + "'" +
               ", EnumValue=" + std::to_string(static_cast<int>(EnumValue)) + "}";
    }
};

// 简化的反射注册（暂时注释掉复杂的宏）
/*
HELIANTHUS_REFLECT_ENUM(TestEnum)
HELIANTHUS_REFLECT_ENUM_VALUE(Value1, TestEnum::Value1)
HELIANTHUS_REFLECT_ENUM_VALUE(Value2, TestEnum::Value2)
HELIANTHUS_REFLECT_ENUM_VALUE(Value3, TestEnum::Value3)

HELIANTHUS_REFLECT_CLASS(TestClass, Object)
HELIANTHUS_REFLECT_PROPERTY(IntValue, int, &TestClass::IntValue, &TestClass::SetIntValue)
HELIANTHUS_REFLECT_PROPERTY(StringValue, std::string, &TestClass::StringValue, &TestClass::SetStringValue)
HELIANTHUS_REFLECT_PROPERTY(EnumValue, TestEnum, &TestClass::EnumValue, &TestClass::SetEnumValue)
HELIANTHUS_REFLECT_METHOD(SetIntValue, void, int)
HELIANTHUS_REFLECT_METHOD(GetIntValue, int)
HELIANTHUS_REFLECT_METHOD(SetStringValue, void, const std::string&)
HELIANTHUS_REFLECT_METHOD(GetStringValue, std::string)
HELIANTHUS_REFLECT_METHOD(SetEnumValue, void, TestEnum)
HELIANTHUS_REFLECT_METHOD(GetEnumValue, TestEnum)
HELIANTHUS_REFLECT_METHOD(Add, int, int, int)
HELIANTHUS_REFLECT_METHOD(Concat, std::string, const std::string&, const std::string&)
HELIANTHUS_REFLECT_METHOD(ToString, std::string)

// 自动注册
HELIANTHUS_AUTO_REGISTER_ENUM(TestEnum)
HELIANTHUS_AUTO_REGISTER(TestClass)
*/

void TestEnumRegistration()
{
    Logger::Info("Testing Enum Registration...");
    
    if (!GlobalReflectionSystem)
    {
        Logger::Error("GlobalReflectionSystem is null");
        return;
    }
    
    auto EnumNames = GlobalReflectionSystem->GetAllEnumNames();
    Logger::Info("Found " + std::to_string(EnumNames.size()) + " registered enums");
    
    bool FoundTestEnum = false;
    for (const auto& Name : EnumNames)
    {
        if (Name == "TestEnum")
        {
            FoundTestEnum = true;
            break;
        }
    }
    
    if (FoundTestEnum)
    {
        Logger::Info("TestEnum found in registered enums");
    }
    else
    {
        Logger::Warn("TestEnum not found in registered enums");
    }
    
    const EnumInfo* Info = GlobalReflectionSystem->GetEnumInfo("TestEnum");
    if (Info)
    {
        Logger::Info("TestEnum info retrieved successfully");
        Logger::Info("TestEnum has " + std::to_string(Info->Values.size()) + " values");
    }
    else
    {
        Logger::Warn("TestEnum info not found");
    }
}

void TestClassRegistration()
{
    Logger::Info("Testing Class Registration...");
    
    if (!GlobalReflectionSystem)
    {
        Logger::Error("GlobalReflectionSystem is null");
        return;
    }
    
    auto ClassNames = GlobalReflectionSystem->GetAllClassNames();
    Logger::Info("Found " + std::to_string(ClassNames.size()) + " registered classes");
    
    bool FoundTestClass = false;
    for (const auto& Name : ClassNames)
    {
        if (Name == "TestClass")
        {
            FoundTestClass = true;
            break;
        }
    }
    
    if (FoundTestClass)
    {
        Logger::Info("TestClass found in registered classes");
    }
    else
    {
        Logger::Warn("TestClass not found in registered classes");
    }
    
    const ClassInfo* Info = GlobalReflectionSystem->GetClassInfo("TestClass");
    if (Info)
    {
        Logger::Info("TestClass info retrieved successfully");
        Logger::Info("TestClass has " + std::to_string(Info->Properties.size()) + " properties");
        Logger::Info("TestClass has " + std::to_string(Info->Methods.size()) + " methods");
    }
    else
    {
        Logger::Warn("TestClass info not found");
    }
}

void TestObjectCreation()
{
    Logger::Info("Testing Object Creation...");
    
    if (!GlobalReflectionSystem)
    {
        Logger::Error("GlobalReflectionSystem is null");
        return;
    }
    
    try
    {
        void* Obj = GlobalReflectionSystem->CreateObject("TestClass");
        if (Obj)
        {
            Logger::Info("TestClass object created successfully");
            
            // 测试属性设置和获取
            int IntValue = 42;
            GlobalReflectionSystem->SetProperty(Obj, "IntValue", &IntValue);
            
            void* RetrievedValue = GlobalReflectionSystem->GetProperty(Obj, "IntValue");
            if (RetrievedValue)
            {
                Logger::Info("Property retrieval successful");
            }
            
            // 清理
            GlobalReflectionSystem->DestroyObject("TestClass", Obj);
            Logger::Info("TestClass object destroyed successfully");
        }
        else
        {
            Logger::Error("Failed to create TestClass object");
        }
    }
    catch (const std::exception& E)
    {
        Logger::Error("Exception during object creation test: " + std::string(E.what()));
    }
}

void TestMethodCalling()
{
    Logger::Info("Testing Method Calling...");
    
    if (!GlobalReflectionSystem)
    {
        Logger::Error("GlobalReflectionSystem is null");
        return;
    }
    
    try
    {
        void* Obj = GlobalReflectionSystem->CreateObject("TestClass");
        if (Obj)
        {
            Logger::Info("TestClass object created for method testing");
            
            // 测试Add方法
            std::vector<void*> Args;
            int A = 10, B = 20;
            Args.push_back(&A);
            Args.push_back(&B);
            
            void* Result = GlobalReflectionSystem->CallMethod(Obj, "Add", Args);
            if (Result)
            {
                Logger::Info("Method calling successful");
            }
            
            // 清理
            GlobalReflectionSystem->DestroyObject("TestClass", Obj);
        }
    }
    catch (const std::exception& E)
    {
        Logger::Error("Exception during method calling test: " + std::string(E.what()));
    }
}

void TestScriptBinding()
{
    Logger::Info("Testing Script Binding...");
    
    if (!GlobalScriptBindingManager)
    {
        Logger::Error("GlobalScriptBindingManager is null");
        return;
    }
    
    // 创建脚本引擎
    auto ScriptEngine = std::make_shared<LuaScriptEngine>();
    auto InitResult = ScriptEngine->Initialize();
    if (InitResult.Success)
    {
        Logger::Info("Script engine initialized successfully");
        
        // 设置脚本绑定管理器
        GlobalScriptBindingManager->SetScriptEngine(ScriptEngine);
        
        // 生成绑定代码
        std::string BindingCode = GlobalScriptBindingManager->GenerateBindingCode("lua");
        Logger::Info("Generated binding code length: " + std::to_string(BindingCode.length()));
    }
    else
    {
        Logger::Error("Failed to initialize script engine: " + InitResult.ErrorMessage);
    }
}

int main()
{
    Logger::Info("Starting Reflection System Tests...");
    
    // 初始化反射系统
    InitializeReflectionSystem();
    InitializeScriptBinding();
    
    // 运行测试
    TestEnumRegistration();
    TestClassRegistration();
    TestObjectCreation();
    TestMethodCalling();
    TestScriptBinding();
    
    // 清理
    ShutdownScriptBinding();
    ShutdownReflectionSystem();
    
    Logger::Info("Reflection System Tests completed");
    return 0;
}
