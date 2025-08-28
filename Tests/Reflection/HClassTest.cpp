#include <gtest/gtest.h>
#include "Shared/Reflection/HClassPascal.h"
#include "Shared/Reflection/MetaSystem.h"

using namespace Helianthus::Reflection;
using namespace Helianthus::Reflection::Meta;

// 测试类
HCLASS(Scriptable | BlueprintType | Category="Test")
class TestPlayer : public HObject
{
public:
    HPROPERTY(ScriptReadable | BlueprintReadOnly | Category="Stats")
    int Level = 1;

    HPROPERTY(SaveGame | BlueprintReadWrite | Category="Economy")
    int Gold = 100;

    HPROPERTY(Config | Category="Settings")
    std::string PlayerName = "TestPlayer";
    
    HFUNCTION(ScriptCallable | BlueprintCallable | Category="Leveling")
    void LevelUp()
    {
        Level++;
        Gold += 50;
    }

    HFUNCTION(BlueprintCallable | BlueprintPure | Category="Info")
    int GetTotalWealth() const
    {
        return Gold;
    }

    HFUNCTION(ScriptCallable | Category="Actions")
    std::string GetPlayerInfo() const
    {
        return "Player: " + PlayerName + " Level: " + std::to_string(Level) + " Gold: " + std::to_string(Gold);
    }
};

// 手动注册测试类
class HClassTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterTestClass();
    }
    
    void RegisterTestClass()
    {
        Meta::ReflectedClass Class;
        Class.Name = "TestPlayer";
        Class.SuperClassName = "HObject";
        
        // 注册类Meta
        Class.Meta.AddTag("Scriptable");
        Class.Meta.AddTag("BlueprintType");
        Class.Meta.AddTag("Category", "Test");
        
        // 注册属性
        Meta::ReflectedProperty LevelProperty;
        LevelProperty.Name = "Level";
        LevelProperty.Type = "int";
        LevelProperty.Offset = offsetof(TestPlayer, Level);
        LevelProperty.Meta.AddTag("ScriptReadable");
        LevelProperty.Meta.AddTag("BlueprintReadOnly");
        LevelProperty.Meta.AddTag("Category", "Stats");
        
        Meta::ReflectedProperty GoldProperty;
        GoldProperty.Name = "Gold";
        GoldProperty.Type = "int";
        GoldProperty.Offset = offsetof(TestPlayer, Gold);
        GoldProperty.Meta.AddTag("SaveGame");
        GoldProperty.Meta.AddTag("BlueprintReadWrite");
        GoldProperty.Meta.AddTag("Category", "Economy");
        
        Meta::ReflectedProperty NameProperty;
        NameProperty.Name = "PlayerName";
        NameProperty.Type = "std::string";
        NameProperty.Offset = offsetof(TestPlayer, PlayerName);
        NameProperty.Meta.AddTag("Config");
        NameProperty.Meta.AddTag("Category", "Settings");
        
        Class.Properties.push_back(LevelProperty);
        Class.Properties.push_back(GoldProperty);
        Class.Properties.push_back(NameProperty);
        
        // 注册函数
        Meta::ReflectedFunction LevelUpFunction;
        LevelUpFunction.Name = "LevelUp";
        LevelUpFunction.ReturnType = "void";
        LevelUpFunction.Meta.AddTag("ScriptCallable");
        LevelUpFunction.Meta.AddTag("BlueprintCallable");
        LevelUpFunction.Meta.AddTag("Category", "Leveling");
        
        Meta::ReflectedFunction GetWealthFunction;
        GetWealthFunction.Name = "GetTotalWealth";
        GetWealthFunction.ReturnType = "int";
        GetWealthFunction.Meta.AddTag("BlueprintCallable");
        GetWealthFunction.Meta.AddTag("BlueprintPure");
        GetWealthFunction.Meta.AddTag("Category", "Info");
        GetWealthFunction.IsConst = true;
        
        Meta::ReflectedFunction GetInfoFunction;
        GetInfoFunction.Name = "GetPlayerInfo";
        GetInfoFunction.ReturnType = "std::string";
        GetInfoFunction.Meta.AddTag("ScriptCallable");
        GetInfoFunction.Meta.AddTag("Category", "Actions");
        GetInfoFunction.IsConst = true;
        
        Class.Functions.push_back(LevelUpFunction);
        Class.Functions.push_back(GetWealthFunction);
        Class.Functions.push_back(GetInfoFunction);
        
        Meta::ReflectionRegistry::Get().RegisterClass(Class);
    }
};

TEST_F(HClassTest, ClassRegistration)
{
    const Meta::ReflectedClass* Class = Meta::ReflectionRegistry::Get().GetClass("TestPlayer");
    ASSERT_NE(Class, nullptr);
    EXPECT_EQ(Class->Name, "TestPlayer");
    EXPECT_EQ(Class->SuperClassName, "HObject");
    
    EXPECT_TRUE(Class->Meta.HasTag("Scriptable"));
    EXPECT_TRUE(Class->Meta.HasTag("BlueprintType"));
    EXPECT_EQ(Class->Meta.GetTag("Category"), "Test");
}

TEST_F(HClassTest, PropertyRegistration)
{
    const Meta::ReflectedProperty* LevelProperty = Meta::ReflectionRegistry::Get().GetProperty("TestPlayer", "Level");
    ASSERT_NE(LevelProperty, nullptr);
    EXPECT_EQ(LevelProperty->Name, "Level");
    EXPECT_EQ(LevelProperty->Type, "int");
    EXPECT_EQ(LevelProperty->Offset, offsetof(TestPlayer, Level));
    
    EXPECT_TRUE(LevelProperty->Meta.HasTag("ScriptReadable"));
    EXPECT_TRUE(LevelProperty->Meta.HasTag("BlueprintReadOnly"));
    EXPECT_EQ(LevelProperty->Meta.GetTag("Category"), "Stats");
}

TEST_F(HClassTest, FunctionRegistration)
{
    const Meta::ReflectedFunction* LevelUpFunction = Meta::ReflectionRegistry::Get().GetFunction("TestPlayer", "LevelUp");
    ASSERT_NE(LevelUpFunction, nullptr);
    EXPECT_EQ(LevelUpFunction->Name, "LevelUp");
    EXPECT_EQ(LevelUpFunction->ReturnType, "void");
    
    EXPECT_TRUE(LevelUpFunction->Meta.HasTag("ScriptCallable"));
    EXPECT_TRUE(LevelUpFunction->Meta.HasTag("BlueprintCallable"));
    EXPECT_EQ(LevelUpFunction->Meta.GetTag("Category"), "Leveling");
}

TEST_F(HClassTest, PropertyAccess)
{
    TestPlayer Player;
    EXPECT_EQ(Player.Level, 1);
    EXPECT_EQ(Player.Gold, 100);
    EXPECT_EQ(Player.PlayerName, "TestPlayer");
    
    Player.LevelUp();
    EXPECT_EQ(Player.Level, 2);
    EXPECT_EQ(Player.Gold, 150);
}

TEST_F(HClassTest, FunctionExecution)
{
    TestPlayer Player;
    
    EXPECT_EQ(Player.GetTotalWealth(), 100);
    
    Player.LevelUp();
    EXPECT_EQ(Player.GetTotalWealth(), 150);
    
    std::string Info = Player.GetPlayerInfo();
    EXPECT_TRUE(Info.find("Player: TestPlayer") != std::string::npos);
    EXPECT_TRUE(Info.find("Level: 2") != std::string::npos);
    EXPECT_TRUE(Info.find("Gold: 150") != std::string::npos);
}

TEST_F(HClassTest, ListOperations)
{
    auto ClassNames = Meta::ReflectionRegistry::Get().GetClassNames();
    EXPECT_FALSE(ClassNames.empty());
    EXPECT_NE(std::find(ClassNames.begin(), ClassNames.end(), "TestPlayer"), ClassNames.end());
    
    auto PropertyNames = Meta::ReflectionRegistry::Get().GetPropertyNames("TestPlayer");
    EXPECT_EQ(PropertyNames.size(), 3);
    EXPECT_NE(std::find(PropertyNames.begin(), PropertyNames.end(), "Level"), PropertyNames.end());
    EXPECT_NE(std::find(PropertyNames.begin(), PropertyNames.end(), "Gold"), PropertyNames.end());
    EXPECT_NE(std::find(PropertyNames.begin(), PropertyNames.end(), "PlayerName"), PropertyNames.end());
    
    auto FunctionNames = Meta::ReflectionRegistry::Get().GetFunctionNames("TestPlayer");
    EXPECT_EQ(FunctionNames.size(), 3);
    EXPECT_NE(std::find(FunctionNames.begin(), FunctionNames.end(), "LevelUp"), FunctionNames.end());
    EXPECT_NE(std::find(FunctionNames.begin(), FunctionNames.end(), "GetTotalWealth"), FunctionNames.end());
    EXPECT_NE(std::find(FunctionNames.begin(), FunctionNames.end(), "GetPlayerInfo"), FunctionNames.end());
}