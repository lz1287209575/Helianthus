#pragma once

#ifdef _WIN32

    #include "Shared/Network/Asio/Reactor.h"

    #include <unordered_map>

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>

namespace Helianthus::Network::Asio
{
class ReactorIocp : public Reactor
{
public:
    ReactorIocp();
    ~ReactorIocp() override;

    bool Add(Fd Handle, EventMask Mask, IoCallback Callback) override;
    bool Mod(Fd Handle, EventMask Mask) override;
    bool Del(Fd Handle) override;
    int PollOnce(int TimeoutMs) override;
    
    // 批处理相关方法
    int PollBatch(int TimeoutMs, size_t MaxEvents = 64) override;
    void SetBatchConfig(const BatchConfig& Config) override;
    BatchConfig GetBatchConfig() const override;
    
    // 性能统计
    PerformanceStats GetPerformanceStats() const override;
    void ResetPerformanceStats() override;

private:
    HANDLE IocpHandle;
    std::unordered_map<Fd, IoCallback> Callbacks;
    std::unordered_map<Fd, EventMask> Masks;
};
}  // namespace Helianthus::Network::Asio

#endif  // _WIN32
