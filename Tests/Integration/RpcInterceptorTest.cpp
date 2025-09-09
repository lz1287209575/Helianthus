#include <gtest/gtest.h>
#include "Shared/RPC/RpcMessageHandler.h"
#include "Shared/RPC/RpcInterceptors.h"
#include "Shared/RPC/RpcTypes.h"
#include "Shared/RPC/RpcMessage.h"

using namespace Helianthus::RPC;

class RpcInterceptorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Handler = std::make_unique<RpcMessageHandler>();
    }

    void TearDown() override
    {
        Handler.reset();
    }

    std::unique_ptr<RpcMessageHandler> Handler;
};

TEST_F(RpcInterceptorTest, BasicInterceptorChain)
{
    // Create a simple test interceptor
    class TestInterceptor : public IRpcInterceptor
    {
    public:
        bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) override
        {
            BeforeCallCount++;
            return true;
        }

        void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) override
        {
            AfterCallCount++;
        }

        void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) override
        {
            ErrorCount++;
        }

        std::string GetName() const override { return "TestInterceptor"; }
        int GetPriority() const override { return 100; }

        int BeforeCallCount = 0;
        int AfterCallCount = 0;
        int ErrorCount = 0;
    };

    auto TestInterceptorPtr = std::make_shared<TestInterceptor>();
    Handler->AddInterceptor(TestInterceptorPtr);

    // Create a test message
    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    // Verify interceptor was called
    EXPECT_EQ(TestInterceptorPtr->BeforeCallCount, 1);
    EXPECT_EQ(TestInterceptorPtr->AfterCallCount, 1);
    EXPECT_EQ(TestInterceptorPtr->ErrorCount, 0);
    EXPECT_EQ(Result, RpcResult::SUCCESS);
}

TEST_F(RpcInterceptorTest, LoggingInterceptor)
{
    auto LoggingInterceptorPtr = std::make_shared<LoggingInterceptor>(true, true, true);
    Handler->AddInterceptor(LoggingInterceptorPtr);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
    EXPECT_EQ(Handler->GetInterceptorChain().GetInterceptorCount(), 1);
}

TEST_F(RpcInterceptorTest, PerformanceInterceptor)
{
    auto PerformanceInterceptorPtr = std::make_shared<PerformanceInterceptor>();
    Handler->AddInterceptor(PerformanceInterceptorPtr);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
    
    PerformanceInterceptor::PerformanceStats Stats;
    PerformanceInterceptorPtr->GetStats(Stats);
    EXPECT_EQ(Stats.TotalCalls.load(), 1);
    EXPECT_EQ(Stats.SuccessfulCalls.load(), 1);
    EXPECT_EQ(Stats.FailedCalls.load(), 0);
}

TEST_F(RpcInterceptorTest, AuthenticationInterceptor)
{
    bool AuthCalled = false;
    auto AuthInterceptorPtr = std::make_shared<AuthenticationInterceptor>(
        [&AuthCalled](const RpcContext& Context, const RpcMessage& Message) -> bool {
            AuthCalled = true;
            return true;
        }
    );
    
    Handler->AddInterceptor(AuthInterceptorPtr);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
    EXPECT_TRUE(AuthCalled);
}

TEST_F(RpcInterceptorTest, RateLimitInterceptor)
{
    auto RateLimitInterceptorPtr = std::make_shared<RateLimitInterceptor>(10, 5);
    Handler->AddInterceptor(RateLimitInterceptorPtr);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    // First few calls should succeed
    for (int i = 0; i < 5; ++i)
    {
        RpcMessage Response;
        RpcResult Result = Handler->ProcessMessage(Message, Response);
        EXPECT_EQ(Result, RpcResult::SUCCESS);
    }
}

TEST_F(RpcInterceptorTest, CacheInterceptor)
{
    auto CacheInterceptorPtr = std::make_shared<CacheInterceptor>(300);
    Handler->AddInterceptor(CacheInterceptorPtr);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
    EXPECT_EQ(CacheInterceptorPtr->GetCacheSize(), 1);
}

TEST_F(RpcInterceptorTest, MultipleInterceptors)
{
    auto LoggingInterceptorPtr = std::make_shared<LoggingInterceptor>();
    auto PerformanceInterceptorPtr = std::make_shared<PerformanceInterceptor>();
    auto AuthInterceptorPtr = std::make_shared<AuthenticationInterceptor>(
        [](const RpcContext&, const RpcMessage&) -> bool { return true; }
    );

    Handler->AddInterceptor(LoggingInterceptorPtr);
    Handler->AddInterceptor(PerformanceInterceptorPtr);
    Handler->AddInterceptor(AuthInterceptorPtr);

    EXPECT_EQ(Handler->GetInterceptorChain().GetInterceptorCount(), 3);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
}

TEST_F(RpcInterceptorTest, InterceptorRemoval)
{
    auto LoggingInterceptorPtr = std::make_shared<LoggingInterceptor>();
    Handler->AddInterceptor(LoggingInterceptorPtr);

    EXPECT_EQ(Handler->GetInterceptorChain().GetInterceptorCount(), 1);
    EXPECT_TRUE(Handler->GetInterceptorChain().HasInterceptor("LoggingInterceptor"));

    Handler->RemoveInterceptor("LoggingInterceptor");

    EXPECT_EQ(Handler->GetInterceptorChain().GetInterceptorCount(), 0);
    EXPECT_FALSE(Handler->GetInterceptorChain().HasInterceptor("LoggingInterceptor"));
}

TEST_F(RpcInterceptorTest, InterceptorPriority)
{
    class HighPriorityInterceptor : public IRpcInterceptor
    {
    public:
        bool OnBeforeCall(RpcContext&, const RpcMessage&) override { return true; }
        void OnAfterCall(RpcContext&, const RpcMessage&, const RpcResult&) override {}
        void OnError(RpcContext&, const RpcMessage&, const std::string&) override {}
        std::string GetName() const override { return "HighPriority"; }
        int GetPriority() const override { return 10; }
    };

    class LowPriorityInterceptor : public IRpcInterceptor
    {
    public:
        bool OnBeforeCall(RpcContext&, const RpcMessage&) override { return true; }
        void OnAfterCall(RpcContext&, const RpcMessage&, const RpcResult&) override {}
        void OnError(RpcContext&, const RpcMessage&, const std::string&) override {}
        std::string GetName() const override { return "LowPriority"; }
        int GetPriority() const override { return 200; }
    };

    auto LowPriorityInterceptorPtr = std::make_shared<LowPriorityInterceptor>();
    auto HighPriorityInterceptorPtr = std::make_shared<HighPriorityInterceptor>();

    // Add in reverse order to test sorting
    Handler->AddInterceptor(LowPriorityInterceptorPtr);
    Handler->AddInterceptor(HighPriorityInterceptorPtr);

    EXPECT_EQ(Handler->GetInterceptorChain().GetInterceptorCount(), 2);

    RpcContext Context;
    Context.ServiceName = "test_service";
    Context.MethodName = "test_method";
    Context.CallType = RpcCallType::HEARTBEAT;
    
    RpcMessage Message(Context);

    RpcMessage Response;
    RpcResult Result = Handler->ProcessMessage(Message, Response);

    EXPECT_EQ(Result, RpcResult::SUCCESS);
}
