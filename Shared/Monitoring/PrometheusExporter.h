#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace Helianthus::Monitoring
{

class PrometheusExporter
{
public:
    using MetricsProvider = std::function<std::string()>;

    PrometheusExporter();
    ~PrometheusExporter();

    bool Start(uint16_t Port, MetricsProvider Provider);
    void Stop();
    bool IsRunning() const
    {
        return Running.load();
    }

private:
    void ServerLoop(uint16_t Port);

    MetricsProvider Provider;
    std::thread ServerThread;
    std::atomic<bool> Running{false};
};

}  // namespace Helianthus::Monitoring
