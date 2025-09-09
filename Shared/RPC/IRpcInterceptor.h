#pragma once

#include "RpcTypes.h"
#include "RpcMessage.h"

#include <memory>
#include <functional>

namespace Helianthus::RPC
{
/**
 * @brief RPC拦截器接口
 * 
 * 拦截器允许在RPC调用前后执行自定义逻辑，如日志记录、认证、监控、缓存等
 */
class IRpcInterceptor
{
public:
    virtual ~IRpcInterceptor() = default;

    /**
     * @brief 在RPC调用前执行
     * @param Context RPC上下文
     * @param Message RPC消息
     * @return 是否继续执行RPC调用
     */
    virtual bool OnBeforeCall(RpcContext& Context, const RpcMessage& Message) = 0;

    /**
     * @brief 在RPC调用后执行
     * @param Context RPC上下文
     * @param Message RPC消息
     * @param Result RPC结果
     */
    virtual void OnAfterCall(RpcContext& Context, const RpcMessage& Message, const RpcResult& Result) = 0;

    /**
     * @brief 在RPC调用异常时执行
     * @param Context RPC上下文
     * @param Message RPC消息
     * @param Error 错误信息
     */
    virtual void OnError(RpcContext& Context, const RpcMessage& Message, const std::string& Error) = 0;

    /**
     * @brief 获取拦截器名称
     * @return 拦截器名称
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief 获取拦截器优先级（数值越小优先级越高）
     * @return 优先级
     */
    virtual int GetPriority() const = 0;
};

using RpcInterceptorPtr = std::shared_ptr<IRpcInterceptor>;

/**
 * @brief 拦截器链管理器
 */
class RpcInterceptorChain
{
public:
    /**
     * @brief 添加拦截器
     * @param Interceptor 拦截器实例
     */
    void AddInterceptor(RpcInterceptorPtr Interceptor);

    /**
     * @brief 移除拦截器
     * @param Name 拦截器名称
     */
    void RemoveInterceptor(const std::string& Name);

    /**
     * @brief 清空所有拦截器
     */
    void Clear();

    /**
     * @brief 执行拦截器链
     * @param Context RPC上下文
     * @param Message RPC消息
     * @param Handler 实际的RPC处理函数
     * @return RPC结果
     */
    RpcResult Execute(RpcContext& Context, const RpcMessage& Message, 
                     std::function<RpcResult(const RpcMessage&)> Handler);

    /**
     * @brief 获取拦截器数量
     * @return 拦截器数量
     */
    size_t GetInterceptorCount() const;

    /**
     * @brief 检查是否包含指定拦截器
     * @param Name 拦截器名称
     * @return 是否包含
     */
    bool HasInterceptor(const std::string& Name) const;

private:
    std::vector<RpcInterceptorPtr> Interceptors;
    mutable std::mutex InterceptorsMutex;

    void SortInterceptors();
};

} // namespace Helianthus::RPC
