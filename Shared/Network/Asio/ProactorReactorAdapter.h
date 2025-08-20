#pragma once

#include "Shared/Network/Asio/Proactor.h"
#include "Shared/Network/Asio/Reactor.h"

#include <memory>

namespace Helianthus::Network::Asio
{
// 通过 Reactor 适配实现 Proactor：当可读/可写时触发一次性回调
class ProactorReactorAdapter : public Proactor
{
public:
    explicit ProactorReactorAdapter(std::shared_ptr<Reactor> ReactorIn);

    void AsyncRead(Fd Handle, char* Buffer, size_t BufferSize, CompletionHandler Handler) override;
    void AsyncWrite(Fd Handle, const char* Data, size_t Size, CompletionHandler Handler) override;

private:
    std::shared_ptr<Reactor> ReactorPtr;
};
}  // namespace Helianthus::Network::Asio
