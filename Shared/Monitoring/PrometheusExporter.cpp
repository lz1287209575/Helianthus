#include "PrometheusExporter.h"
#include "Shared/Common/LogCategories.h"
#include <Network/Sockets/TcpSocket.h>
#include <Network/NetworkTypes.h>
#include <picohttpparser.h>

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
    if (Server.Bind(Bind) != NetworkError::NONE) { 
        std::cout << "Bind failed" << std::endl;
        Running.store(false); 
        return; 
    }
    if (Server.Listen(64) != NetworkError::NONE) { 
        std::cout << "Listen failed" << std::endl;
        Running.store(false); 
        return; 
    }
    std::cout << "Server started successfully on port " << Port << std::endl;

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
            std::cout << "收到请求: " << Req << std::endl;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 非阻塞模式下没有数据可读，等待一下再试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else
        {
            std::cout << "接收失败: err=" << static_cast<int>(ErrRecv) << ", recv=" << Recv << ", errno=" << errno << std::endl;
            continue;
        }
        
        // 使用 picohttpparser 正确解析
        const char* MethodC = nullptr; 
        size_t MethodLen = 0;
        const char* PathC = nullptr; 
        size_t PathLen = 0; 
        int Minor = 1;
        
        std::cout << "开始解析请求，长度: " << Req.size() << std::endl;
        std::cout << "请求内容: [" << Req << "]" << std::endl;
        
        // 使用正确的 phr_parse_request 调用
        struct phr_header Headers[16];
        size_t NumHeaders = 16; // 设置为数组大小
        int ParsedBytes = phr_parse_request(Req.data(), Req.size(), &MethodC, &MethodLen,
                                            &PathC, &PathLen, &Minor, Headers, &NumHeaders, 0);
        
        std::string Method, Path;
        bool Parsed = ParsedBytes >= 0;
        
        if (Parsed)
        {
            Method.assign(MethodC, MethodLen);
            Path.assign(PathC, PathLen);
            std::cout << "解析成功: method=" << Method << ", path=" << Path << std::endl;
        }
        else
        {
            std::cout << "解析失败: bytes=" << ParsedBytes << std::endl;
            // 如果 picohttpparser 失败，使用简单解析作为后备
            size_t FirstSpace = Req.find(' ');
            if (FirstSpace != std::string::npos)
            {
                Method = Req.substr(0, FirstSpace);
                size_t SecondSpace = Req.find(' ', FirstSpace + 1);
                if (SecondSpace != std::string::npos)
                {
                    Path = Req.substr(FirstSpace + 1, SecondSpace - FirstSpace - 1);
                    Parsed = true;
                    std::cout << "后备解析成功: method=" << Method << ", path=" << Path << std::endl;
                }
            }
        }

        std::string Resp;
        std::cout << "开始构建响应" << std::endl;
        if (!Parsed)
        {
            const char* Msg = "Bad Request";
            Resp = std::string("HTTP/1.1 400 Bad Request\r\nContent-Length: ") +
                   std::to_string(strlen(Msg)) + "\r\nConnection: close\r\n\r\n" + Msg;
            std::cout << "构建了 Bad Request 响应" << std::endl;
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
        std::cout << "准备发送响应，长度: " << Resp.size() << std::endl;
        auto SendResult = Client.Send(Resp.data(), Resp.size(), Sent);
        std::cout << "发送结果: " << static_cast<int>(SendResult) << ", 发送字节: " << Sent << std::endl;
        Client.Disconnect();
        std::cout << "连接已断开" << std::endl;
    }
}

}


