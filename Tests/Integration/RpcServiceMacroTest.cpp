#include <gtest/gtest.h>

#include "Shared/RPC/RpcMessageHandler.h"
#include "Shared/RPC/RpcServiceMacros.h"
#include "Shared/RPC/IRpcServer.h"

using namespace Helianthus::RPC;

// 使用宏定义一个最小服务，包含同步与异步方法
H_RPC_SERVICE_BEGIN(MacroEchoService, "MacroEchoService")
    H_RPC_METHOD(echo, Params)
    {
        // 直接原样返回
        return Params;
    }
    H_RPC_METHOD_END

    H_RPC_ASYNC_METHOD(asyncEcho, Ctx, Params, Callback)
    {
        // 直接通过回调返回
        Callback(RpcResult::SUCCESS, Params);
    }
    H_RPC_ASYNC_METHOD_END
H_RPC_SERVICE_END

class RpcServiceMacroTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Handler = std::make_unique<RpcMessageHandler>();
        Service = std::make_shared<MacroEchoService>();
        Handler->RegisterService(Service);
    }

    std::unique_ptr<RpcMessageHandler> Handler;
    std::shared_ptr<IRpcService> Service;
};

TEST_F(RpcServiceMacroTest, SyncMethodEcho)
{
    RpcContext Ctx;
    Ctx.ServiceName = "MacroEchoService";
    Ctx.MethodName = "echo";
    Ctx.CallType = RpcCallType::REQUEST;

    RpcMessage Req(Ctx);
    Req.SetParameters("{\"msg\":\"hello\"}");

    RpcMessage Resp;
    RpcResult Res = Handler->ProcessMessage(Req, Resp);

    EXPECT_EQ(Res, RpcResult::SUCCESS);
}

// 强类型方法服务
H_RPC_SERVICE_BEGIN(TypedService, "TypedService")
    struct AddReq { int A{1}; int B{2}; };
    struct AddResp { int Sum{0}; };

    H_RPC_TYPED_METHOD(add, AddReq, AddResp)
    {
        Response.Sum = Request.A + Request.B; // 简单示例：底层模板未做真实反序列化
        return RpcResult::SUCCESS;
    }
    H_RPC_TYPED_METHOD_END
H_RPC_SERVICE_END

TEST_F(RpcServiceMacroTest, TypedMethod)
{
    auto Svc = std::make_shared<TypedService>();
    Handler->RegisterService(Svc);

    RpcContext Ctx;
    Ctx.ServiceName = "TypedService";
    Ctx.MethodName = "add";
    Ctx.CallType = RpcCallType::REQUEST;

    RpcMessage Req(Ctx);
    Req.SetParameters("{}");

    RpcMessage Resp;
    RpcResult Res = Handler->ProcessMessage(Req, Resp);
    EXPECT_EQ(Res, RpcResult::SUCCESS);
}

// 自动挂载反射服务的冒烟测试
TEST(RpcAutoMountTest, DISABLED_RegisterReflectedServicesSmoke)
{
    using namespace Helianthus::RPC;
    RpcConfig Config{};
    RpcServer Server(Config);
    // 如果生成器已发现带 Rpc 标签的类且提供 HRPC_FACTORY，会注册到 Server；否则空操作
    RegisterReflectedServices(Server);
    SUCCEED();
}


