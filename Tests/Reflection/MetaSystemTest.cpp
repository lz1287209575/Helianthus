#include <gtest/gtest.h>
#include "Shared/Reflection/MetaSystem.h"

using namespace Helianthus::Reflection::Meta;

class MetaSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ReflectionRegistry::Get().RegisterClass(CreateTestClass());
    }
    
    ReflectedClass CreateTestClass()
    {
        ReflectedClass Class;
        Class.Name = "TestPlayer";
        Class.SuperClassName = "HObject";
        
        // 添加Meta标签
        Class.Meta.AddTag("Scriptable");
        Class.Meta.AddTag("BlueprintType");
        Class.Meta.AddTag("Category", "Player");
        
        // 添加属性
        ReflectedProperty LevelProperty;
        LevelProperty.Name = "Level";
        LevelProperty.Type = "int";
        LevelProperty.Offset = 0;
        LevelProperty.Meta.AddTag("ScriptReadable");
        LevelProperty.Meta.AddTag("BlueprintReadOnly");
        LevelProperty.Meta.AddTag("Category", "Stats");
        
        ReflectedProperty GoldProperty;
        GoldProperty.Name = "Gold";
        GoldProperty.Type = "int";
        GoldProperty.Offset = 4;
        GoldProperty.Meta.AddTag("SaveGame");
        GoldProperty.Meta.AddTag("BlueprintReadWrite");
        GoldProperty.Meta.AddTag("Category", "Economy");
        
        Class.Properties.push_back(LevelProperty);
        Class.Properties.push_back(GoldProperty);
        
        // 添加函数
        ReflectedFunction LevelUpFunction;
        LevelUpFunction.Name = "LevelUp";
        LevelUpFunction.ReturnType = "void";
        LevelUpFunction.Meta.AddTag("ScriptCallable");
        LevelUpFunction.Meta.AddTag("BlueprintCallable");
        LevelUpFunction.Meta.AddTag("Category", "Leveling");
        
        ReflectedFunction GetInfoFunction;
        GetInfoFunction.Name = "GetInfo";
        GetInfoFunction.ReturnType = "std::string";
        GetInfoFunction.Meta.AddTag("BlueprintPure");
        GetInfoFunction.Meta.AddTag("Category", "Info");
        GetInfoFunction.bIsConst = true;
        
        Class.Functions.push_back(LevelUpFunction);
        Class.Functions.push_back(GetInfoFunction);
        
        return Class;
    }
};

TEST_F(MetaSystemTest, MetaTagCreation)
{
    MetaTag Tag("ScriptReadable");
    EXPECT_EQ(Tag.Name, "ScriptReadable");
    EXPECT_TRUE(Tag.Value.empty());
    
    MetaTag TagWithValue("Category", "Player");
    EXPECT_EQ(TagWithValue.Name, "Category");
    EXPECT_EQ(TagWithValue.Value, "Player");
}

TEST_F(MetaSystemTest, MetaTagParameters)
{
    MetaTag Tag("DisplayName");
    Tag.SetParameter("Name", "Player Level");
    Tag.SetParameter("Description", "Current player level");
    
    EXPECT_TRUE(Tag.HasParameter("Name"));
    EXPECT_EQ(Tag.GetParameter("Name"), "Player Level");
    EXPECT_EQ(Tag.GetParameter("Description"), "Current player level");
    EXPECT_EQ(Tag.GetParameter("NonExistent", "Default"), "Default");
}

TEST_F(MetaSystemTest, MetaCollection)
{
    MetaCollection Collection;
    Collection.AddTag("Scriptable");
    Collection.AddTag("BlueprintType");
    Collection.AddTag("Category", "Player");
    
    EXPECT_TRUE(Collection.HasTag("Scriptable"));
    EXPECT_TRUE(Collection.HasTag("BlueprintType"));
    EXPECT_TRUE(Collection.HasTag("Category"));
    EXPECT_FALSE(Collection.HasTag("NonExistent"));
    
    const MetaTag* Tag = Collection.GetTag("Category");
    ASSERT_NE(Tag, nullptr);
    EXPECT_EQ(Tag->Value, "Player");
    
    auto Tags = Collection.GetTags("Category");
    ASSERT_EQ(Tags.size(), 1);
    EXPECT_EQ(Tags[0]->Value, "Player");
}

TEST_F(MetaSystemTest, ReflectionRegistry)
{
    const ReflectedClass* Class = ReflectionRegistry::Get().GetClass("TestPlayer");
    ASSERT_NE(Class, nullptr);
    EXPECT_EQ(Class->Name, "TestPlayer");
    EXPECT_EQ(Class->SuperClassName, "HObject");
    
    EXPECT_TRUE(Class->Meta.HasTag("Scriptable"));
    EXPECT_TRUE(Class->Meta.HasTag("BlueprintType"));
    
    const MetaTag* CategoryTag = Class->Meta.GetTag("Category");
    ASSERT_NE(CategoryTag, nullptr);
    EXPECT_EQ(CategoryTag->Value, "Player");
}

TEST_F(MetaSystemTest, PropertyMeta)
{
    const ReflectedProperty* Property = ReflectionRegistry::Get().GetProperty("TestPlayer", "Level");
    ASSERT_NE(Property, nullptr);
    EXPECT_EQ(Property->Name, "Level");
    EXPECT_EQ(Property->Type, "int");
    
    EXPECT_TRUE(Property->Meta.HasTag("ScriptReadable"));
    EXPECT_TRUE(Property->Meta.HasTag("BlueprintReadOnly"));
    
    const MetaTag* CategoryTag = Property->Meta.GetTag("Category");
    ASSERT_NE(CategoryTag, nullptr);
    EXPECT_EQ(CategoryTag->Value, "Stats");
}

TEST_F(MetaSystemTest, FunctionMeta)
{
    const ReflectedFunction* Function = ReflectionRegistry::Get().GetFunction("TestPlayer", "LevelUp");
    ASSERT_NE(Function, nullptr);
    EXPECT_EQ(Function->Name, "LevelUp");
    EXPECT_EQ(Function->ReturnType, "void");
    
    EXPECT_TRUE(Function->Meta.HasTag("ScriptCallable"));
    EXPECT_TRUE(Function->Meta.HasTag("BlueprintCallable"));
    
    const MetaTag* CategoryTag = Function->Meta.GetTag("Category");
    ASSERT_NE(CategoryTag, nullptr);
    EXPECT_EQ(CategoryTag->Value, "Leveling");
}

TEST_F(MetaSystemTest, ListOperations)
{
    auto ClassNames = ReflectionRegistry::Get().GetClassNames();
    EXPECT_FALSE(ClassNames.empty());
    EXPECT_NE(std::find(ClassNames.begin(), ClassNames.end(), "TestPlayer"), ClassNames.end());
    
    auto PropertyNames = ReflectionRegistry::Get().GetPropertyNames("TestPlayer");
    EXPECT_EQ(PropertyNames.size(), 2);
    EXPECT_NE(std::find(PropertyNames.begin(), PropertyNames.end(), "Level"), PropertyNames.end());
    EXPECT_NE(std::find(PropertyNames.begin(), PropertyNames.end(), "Gold"), PropertyNames.end());
    
    auto FunctionNames = ReflectionRegistry::Get().GetFunctionNames("TestPlayer");
    EXPECT_EQ(FunctionNames.size(), 2);
    EXPECT_NE(std::find(FunctionNames.begin(), FunctionNames.end(), "LevelUp"), FunctionNames.end());
    EXPECT_NE(std::find(FunctionNames.begin(), FunctionNames.end(), "GetInfo"), FunctionNames.end());
}

TEST_F(MetaSystemTest, MetaParser)
{
    std::string MetaString = "Scriptable BlueprintType Category=Player DisplayName=Test Player";
    MetaCollection Collection = MetaParser::ParseMeta(MetaString);
    
    EXPECT_TRUE(Collection.HasTag("Scriptable"));
    EXPECT_TRUE(Collection.HasTag("BlueprintType"));
    EXPECT_TRUE(Collection.HasTag("Category"));
    EXPECT_TRUE(Collection.HasTag("DisplayName"));
    
    const MetaTag* CategoryTag = Collection.GetTag("Category");
    ASSERT_NE(CategoryTag, nullptr);
    EXPECT_EQ(CategoryTag->Value, "Player");
    
    const MetaTag* DisplayNameTag = Collection.GetTag("DisplayName");
    ASSERT_NE(DisplayNameTag, nullptr);
    EXPECT_EQ(DisplayNameTag->Value, "Test");
}

TEST_F(MetaSystemTest, MetaParserWithParameters)
{
    std::string MetaString = "DisplayName=Player Level(Category=Stats,Description=Current level)";
    MetaCollection Collection = MetaParser::ParseMeta(MetaString);
    
    const MetaTag* DisplayNameTag = Collection.GetTag("DisplayName");
    ASSERT_NE(DisplayNameTag, nullptr);
    EXPECT_EQ(DisplayNameTag->Value, "Player Level");
    EXPECT_TRUE(DisplayNameTag->HasParameter("Category"));
    EXPECT_EQ(DisplayNameTag->GetParameter("Category"), "Stats");
    EXPECT_EQ(DisplayNameTag->GetParameter("Description"), "Current level");
}