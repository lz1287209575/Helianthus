#include <gtest/gtest.h>

#include "Shared/Reflection/ReflectionCore.h"
#include "Shared/Reflection/ReflectionMacros.h"

using namespace Helianthus::Reflection;

class HPlayer
{
    GENERATED_BODY()

public:
    void NativeConstruct()
    {
    }

    HPROPERTY(ScriptReadable)
    std::string PlayerName;
    HPROPERTY_AUTO(HPlayer, PlayerName, ScriptReadable)

    HFUNCTION()
    static void TestFunc()
    {
    }

    HMETHOD(ScriptCallable)
    void TestMethod()
    {
    }
    HMETHOD_AUTO(HPlayer, TestMethod, ScriptCallable)

    HMETHOD(ScriptImpl)
    void ScriptImplFunc()
    {
    }
    HMETHOD_AUTO(HPlayer, ScriptImplFunc, ScriptImpl)

    HMETHOD(Rpc)
    void RpcMethod()
    {
    }
    HMETHOD_AUTO(HPlayer, RpcMethod, Rpc)
};

static int __HPlayer_SupplementaryRegistration = []() -> int {
    // Class + tag + factory
    ClassRegistry::Get().RegisterClass("HPlayer", {}, [](){ return new HPlayer(); });
    ClassRegistry::Get().AddClassTag("HPlayer", "ScriptCreateable");
    // Property/methods auto-registered above
    // Static function tagged as function
    ClassRegistry::Get().RegisterMethod("HPlayer", "TestFunc", "Function");
    return 0;
}();

TEST(HPlayerReflectionTest, ClassRegistration)
{
    EXPECT_TRUE(ClassRegistry::Get().Has("HPlayer"));
    auto* Obj = ClassRegistry::Get().Create("HPlayer");
    EXPECT_NE(Obj, nullptr);
    delete static_cast<HPlayer*>(Obj);
}

TEST(HPlayerReflectionTest, PropertyRegistration)
{
    const ClassMeta* Meta = ClassRegistry::Get().Get("HPlayer");
    ASSERT_NE(Meta, nullptr);
    bool Found = false;
    for (const auto& P : Meta->Properties)
    {
        if (P.Name == "PlayerName" && P.Tag == "ScriptReadable")
        {
            Found = true;
            break;
        }
    }
    EXPECT_TRUE(Found);
}

TEST(HPlayerReflectionTest, MethodRegistration)
{
    const ClassMeta* Meta = ClassRegistry::Get().Get("HPlayer");
    ASSERT_NE(Meta, nullptr);
    // Expect presence of three methods with their tags
    auto has = [&](const char* name, const char* tag) {
        for (const auto& M : Meta->Methods)
        {
            if (M.Name == name)
            {
                for (const auto& t : M.Tags)
                {
                    if (t == tag) return true;
                }
            }
        }
        return false;
    };
    EXPECT_TRUE(has("TestMethod", "ScriptCallable"));
    EXPECT_TRUE(has("ScriptImplFunc", "ScriptImpl"));
    EXPECT_TRUE(has("RpcMethod", "Rpc"));
}


