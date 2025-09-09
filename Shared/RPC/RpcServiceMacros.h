#pragma once

#include <functional>
#include <string>

#include "IRpcServer.h"
#include "RpcReflection.h"

// 该文件提供便捷宏，帮助快速定义基于 RpcServiceBase 的服务并注册方法。
// 用法示例：
//
// H_RPC_SERVICE_BEGIN(CalculatorService, "CalculatorService")
//     H_RPC_METHOD(add, params) {
//         return std::string("{}\n");
//     } H_RPC_METHOD_END
// H_RPC_SERVICE_END
//
// 或注册异步方法：
// H_RPC_ASYNC_METHOD(asyncAdd, ctx, params, cb) {
//     cb(Helianthus::RPC::RpcResult::SUCCESS, "{}\n");
// } H_RPC_ASYNC_METHOD_END

// 开始定义一个派生自 RpcServiceBase 的服务类
#define H_RPC_SERVICE_BEGIN(ServiceClassName, ServiceNameLiteral)                                  \
    class ServiceClassName : public Helianthus::RPC::RpcServiceBase                                 \
    {                                                                                               \
    public:                                                                                         \
        ServiceClassName() : Helianthus::RPC::RpcServiceBase(ServiceNameLiteral)                   \
        {                                                                                           \
            /* ensure reflection meta exists */                                                     \
            Helianthus::RPC::RpcServiceRegistry::Get().RegisterService(                            \
                ServiceNameLiteral, GetServiceStaticVersion(),                                     \
                [](){ return std::make_shared<ServiceClassName>(); });                              \
        }                                                                                           \
        ~ServiceClassName() override = default;                                                     \
    private:

    static const char* GetServiceStaticVersion() { return "1.0.0"; }

// 结束服务类定义
#define H_RPC_SERVICE_END                                                                            \
    };

// 注册一个同步方法：方法体需返回 std::string
#define H_RPC_METHOD(MethodName, ParamsIdent)                                                        \
    struct __H_RPC_WRAP_##MethodName                                                                 \
    {                                                                                                \
        template <typename T>                                                                        \
        explicit __H_RPC_WRAP_##MethodName(T* Self)                                                  \
        {                                                                                            \
            Self->RegisterMethod(#MethodName,                                                        \
                [this](const std::string& ParamsIdent) -> std::string { return Invoke(ParamsIdent); }); \
            Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod(                              \
                Self->GetServiceName(),                                                              \
                Helianthus::RPC::RpcMethodMeta{#MethodName, "Sync", "", ""});                    \
        }                                                                                            \
        std::string Invoke(const std::string& ParamsIdent)

#define H_RPC_METHOD_END                                                                             \
    } __H_RPC_WRAP_INSTANCE_##__LINE__ { this };

// 注册一个异步方法：方法体通过回调返回结果
#define H_RPC_ASYNC_METHOD(MethodName, CtxIdent, ParamsIdent, CallbackIdent)                         \
    struct __H_RPC_WRAP_ASYNC_##MethodName                                                           \
    {                                                                                                \
        template <typename T>                                                                        \
        explicit __H_RPC_WRAP_ASYNC_##MethodName(T* Self)                                            \
        {                                                                                            \
            Self->RegisterAsyncMethod(                                                               \
                #MethodName,                                                                         \
                [this](const Helianthus::RPC::RpcContext& CtxIdent,                                  \
                       const std::string& ParamsIdent,                                               \
                       Helianthus::RPC::RpcCallback CallbackIdent) { Invoke(CtxIdent, ParamsIdent, CallbackIdent); }); \
            Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod(                              \
                Self->GetServiceName(),                                                              \
                Helianthus::RPC::RpcMethodMeta{#MethodName, "Async", "", ""});                   \
        }                                                                                            \
        void Invoke(const Helianthus::RPC::RpcContext& CtxIdent,                                     \
                    const std::string& ParamsIdent,                                                  \
                    Helianthus::RPC::RpcCallback CallbackIdent)

#define H_RPC_ASYNC_METHOD_END                                                                       \
    } __H_RPC_WRAP_ASYNC_INSTANCE_##__LINE__ { this };

// 注册强类型同步方法：TReq/TResp 为类型，占位序列化由底层模板处理
#define H_RPC_TYPED_METHOD(MethodName, TReq, TResp)                                                   \
    struct __H_RPC_WRAP_TYPED_##MethodName                                                            \
    {                                                                                                \
        template <typename T>                                                                        \
        explicit __H_RPC_WRAP_TYPED_##MethodName(T* Self)                                            \
        {                                                                                            \
            Self->template RegisterTypedMethod<TReq, TResp>(#MethodName,                             \
                [this](const TReq& Request, TResp& Response) -> Helianthus::RPC::RpcResult {         \
                    return Invoke(Request, Response);                                                \
                });                                                                                  \
            Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod(                              \
                Self->GetServiceName(),                                                              \
                Helianthus::RPC::RpcMethodMeta{#MethodName, "Typed", #TReq, #TResp});               \
        }                                                                                            \
        Helianthus::RPC::RpcResult Invoke(const TReq& Request, TResp& Response)

#define H_RPC_TYPED_METHOD_END                                                                        \
    } __H_RPC_WRAP_TYPED_INSTANCE_##__LINE__ { this };

// 注册强类型异步方法
#define H_RPC_TYPED_ASYNC_METHOD(MethodName, TReq, TResp)                                             \
    struct __H_RPC_WRAP_TYPED_ASYNC_##MethodName                                                      \
    {                                                                                                \
        template <typename T>                                                                        \
        explicit __H_RPC_WRAP_TYPED_ASYNC_##MethodName(T* Self)                                      \
        {                                                                                            \
            Self->template RegisterTypedAsyncMethod<TReq, TResp>(#MethodName,                        \
                [this](const TReq& Request, std::function<void(Helianthus::RPC::RpcResult, const TResp&)> Cb) { \
                    Invoke(Request, Cb);                                                             \
                });                                                                                  \
            Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod(                              \
                Self->GetServiceName(),                                                              \
                Helianthus::RPC::RpcMethodMeta{#MethodName, "TypedAsync", #TReq, #TResp});          \
        }                                                                                            \
        void Invoke(const TReq& Request, std::function<void(Helianthus::RPC::RpcResult, const TResp&)> Cb)

#define H_RPC_TYPED_ASYNC_METHOD_END                                                                  \
    } __H_RPC_WRAP_TYPED_ASYNC_INSTANCE_##__LINE__ { this };


