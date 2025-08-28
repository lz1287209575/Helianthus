#include "Shared/Monitoring/PrometheusExporter.h"

#include "Shared/Network/Sockets/TcpSocket.h"
#include "Shared/Network/NetworkTypes.h"
#include <cstring>
#include "picohttpparser.h"

namespace Helianthus::Monitoring
{

PrometheusExporter::PrometheusExporter() = default;
PrometheusExporter::~PrometheusExporter()
{
    Stop();
}

bool PrometheusExporter::Start(uint16_t Port, MetricsProvider ProviderFn)
{
    if (Running.load()) return true;
    Provider = std::move(ProviderFn);
    Running.store(true);
    ServerThread = std::thread(&PrometheusExporter::ServerLoop, this, Port);
    return true;
}

void PrometheusExporter::Stop()
{
    if (!Running.exchange(false)) return;
    if (ServerThread.joinable()) ServerThread.join();
}

void PrometheusExporter::ServerLoop(uint16_t Port)
{
    using namespace Helianthus::Network;
    using namespace Helianthus::Network::Sockets;
    TcpSocket Server;
    NetworkAddress Bind{"0.0.0.0", Port};
    if (Server.Bind(Bind) != NetworkError::NONE) { Running.store(false); return; }
    if (Server.Listen(64) != NetworkError::NONE) { Running.store(false); return; }

    while (Running.load())
    {
        TcpSocket Client;
        auto Err = Server.AcceptClient(Client);
        if (Err != NetworkError::NONE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // 读取请求行并解析路径（循环读取直到获得首行或超时）
        // 读取数据并用 picohttpparser 解析
        std::string Req;
        char Buffer[2048]; size_t Recv = 0;
        auto ErrRecv = Client.Receive(Buffer, sizeof(Buffer), Recv);
        if (ErrRecv == Helianthus::Network::NetworkError::NONE && Recv > 0)
        {
            Req.assign(Buffer, Buffer + Recv);
        }
        const char* MethodC = nullptr; size_t MethodLen = 0;
        const char* PathC = nullptr; size_t PathLen = 0; int Minor = 1;
        phr_header Headers[16]; size_t NumHeaders = 0;
        int ParsedBytes = phr_parse_request(Req.data(), Req.size(), &MethodC, &MethodLen,
                                            &PathC, &PathLen, &Minor, Headers, &NumHeaders, 0);
        std::string Method;
        std::string Path;
        bool Parsed = ParsedBytes >= 0;
        if (Parsed)
        {
            Method.assign(MethodC, MethodLen);
            Path.assign(PathC, PathLen);
        }

        std::string Resp;
        if (!Parsed)
        {
            const char* Msg = "Bad Request";
            Resp = std::string("HTTP/1.1 400 Bad Request\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
        }
        else if (Path == "/metrics")
        {
            if (Method != "GET" && Method != "HEAD")
            {
                const char* Msg = "Method Not Allowed";
                Resp = std::string("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD\r\nContent-Length: ") +
                       std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
            }
            else
            {
                std::string Body = Provider ? Provider() : std::string();
                if (Body.empty()) Body = "# HELP helianthus_up 1 if exporter is up\n# TYPE helianthus_up gauge\nhelianthus_up 1\n";
                Resp = std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: ") +
                       std::to_string(Body.size()) + "\r\nConnection: close\r\n\r\n" + (Method == "HEAD" ? std::string() : Body);
            }
        }
        else if (Path == "/health")
        {
            const char* Msg = "ok";
            Resp = std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + (Method == "HEAD" ? "" : Msg);
        }
        else
        {
            const char* Msg = "Not Found";
            Resp = std::string("HTTP/1.1 404 Not Found\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
        }

        size_t Sent = 0;
        Client.Send(Resp.data(), Resp.size(), Sent);
        Client.Disconnect();
    }
}

}


