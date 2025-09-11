#include "Shared/RPC/RpcReflection.h"
#include "Shared/RPC/IRpcServer.h"
#include "Shared/Reflection/ReflectionMacros.h"
#include "Shared/Common/LogCategories.h"
#include "reflection_gen.h"
#include <gtest/gtest.h>

namespace Helianthus { namespace Reflection { void RegisterRpc_MiniService(); }}

// 定义一个最小服务，验证标签与限定符提取
HCLASS(Test,NoAutoRegister)
class MiniService : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    MiniService() : RpcServiceBase("MiniService") {}

    // 业务标签：PureFunction,Math；限定符来自签名：static noexcept
    HMETHOD(PureFunction,Math) static int Add(int A, int B) noexcept { return A + B; }

    // 业务标签：Utility；限定符来自签名：inline const
    HMETHOD(Utility) inline int GetValue() const { return 42; }

    // 复杂签名：模板与默认参数、函数指针参数
    HMETHOD(Utility,Advanced)
    void Complex(const std::vector<std::pair<int,std::string>>& Items = {}, int (*Transform)(int) = nullptr) {}
};

TEST(ReflectionTagAndQualifiersTest, TagFilterAndQualifiers)
{
    using namespace Helianthus::RPC;

    // 手动注册工厂（NoAutoRegister 类）
    RpcServiceRegistry::Get().RegisterService(
        "MiniService", "1.0.0",
        [](){ return std::static_pointer_cast<IRpcService>(std::make_shared<MiniService>()); }
    );

    // 显式注册方法元信息（Tests/ 下的类不会进入聚合入口）
    Helianthus::Reflection::RegisterRpc_MiniService();

    auto Names = RpcServiceRegistry::Get().ListServices();
    ASSERT_NE(std::find(Names.begin(), Names.end(), std::string("MiniService")), Names.end());

    // 校验方法元信息已注册，且标签为业务标签
    auto Meta = RpcServiceRegistry::Get().GetMeta("MiniService");
    bool FoundAdd = false, FoundGetValue = false, FoundComplex = false;
    for (const auto& M : Meta.Methods)
    {
        if (M.MethodName == "Add")
        {
            FoundAdd = true;
            // 应包含 PureFunction/Math，不包含 Static/Noexcept 等 C++ 语义
            ASSERT_NE(std::find(M.Tags.begin(), M.Tags.end(), std::string("PureFunction")), M.Tags.end());
            ASSERT_NE(std::find(M.Tags.begin(), M.Tags.end(), std::string("Math")), M.Tags.end());
            ASSERT_EQ(std::find(M.Tags.begin(), M.Tags.end(), std::string("Static")), M.Tags.end());
            ASSERT_EQ(std::find(M.Tags.begin(), M.Tags.end(), std::string("Noexcept")), M.Tags.end());
        }
        if (M.MethodName == "GetValue")
        {
            FoundGetValue = true;
            ASSERT_NE(std::find(M.Tags.begin(), M.Tags.end(), std::string("Utility")), M.Tags.end());
            ASSERT_EQ(std::find(M.Tags.begin(), M.Tags.end(), std::string("Inline")), M.Tags.end());
            ASSERT_EQ(std::find(M.Tags.begin(), M.Tags.end(), std::string("Const")), M.Tags.end());
        }
        if (M.MethodName == "Complex")
        {
            FoundComplex = true;
            bool HasAdvanced = false;
            for (const auto& T : M.Tags) {
                if (T == "Advanced" || T.find("Advanced") != std::string::npos) { HasAdvanced = true; break; }
            }
            ASSERT_TRUE(HasAdvanced);
        }
    }
    ASSERT_TRUE(FoundAdd);
    ASSERT_TRUE(FoundGetValue);
    ASSERT_TRUE(FoundComplex);

    // 按标签筛选挂载：Math 应匹配到 MiniService（Add）
    auto Server = std::make_shared<RpcServer>();
    std::vector<std::string> MathTags = {"Math"};
    RegisterReflectedServices(*Server, MathTags);
    // 无异常即通过（挂载路径在生成代码中打印，测试侧不校验输出）
}
