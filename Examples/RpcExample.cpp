#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "Shared/RPC/IRpcServer.h"
#include "Shared/RPC/IRpcClient.h"
#include "Shared/Network/NetworkTypes.h"
#include "Shared/Common/StructuredLogger.h"

using namespace Helianthus::RPC;
using namespace Helianthus::Network;
using namespace Helianthus::Common;

/**
 * @brief 简单的计算服务实现
 */
class CalculatorService : public RpcServiceBase
{
public:
    CalculatorService() : RpcServiceBase("CalculatorService")
    {
        SetServiceVersion("1.0.0");
        
        // 注册同步方法
        RegisterMethod("add", [this](const std::string& params) -> std::string {
            return HandleAdd(params);
        });
        
        RegisterMethod("subtract", [this](const std::string& params) -> std::string {
            return HandleSubtract(params);
        });
        
        RegisterMethod("multiply", [this](const std::string& params) -> std::string {
            return HandleMultiply(params);
        });
        
        RegisterMethod("divide", [this](const std::string& params) -> std::string {
            return HandleDivide(params);
        });
        
        // 注册异步方法
        RegisterAsyncMethod("asyncAdd", [this](const RpcContext& context, 
                                               const std::string& params, 
                                               RpcCallback callback) {
            HandleAsyncAdd(context, params, callback);
        });
        
        std::cout << "[CalculatorService] 服务初始化完成，注册了 " 
                  << GetMethodNames().size() << " 个方法" << std::endl;
    }
    
    ~CalculatorService() override = default;

private:
    // 简单的JSON解析（实际项目中应该使用proper JSON库）
    bool ParseNumbers(const std::string& params, double& a, double& b) {
        // 简化的JSON解析，格式: {"a": 1, "b": 2}
        size_t aPos = params.find("\"a\":");
        size_t bPos = params.find("\"b\":");
        
        if (aPos == std::string::npos || bPos == std::string::npos) {
            return false;
        }
        
        aPos = params.find_first_of("0123456789.-", aPos);
        bPos = params.find_first_of("0123456789.-", bPos);
        
        if (aPos == std::string::npos || bPos == std::string::npos) {
            return false;
        }
        
        a = std::stod(params.substr(aPos));
        b = std::stod(params.substr(bPos));
        return true;
    }
    
    std::string CreateResult(double result) {
        return "{\"result\": " + std::to_string(result) + "}";
    }
    
    std::string HandleAdd(const std::string& params) {
        double a, b;
        if (!ParseNumbers(params, a, b)) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        double result = a + b;
        std::cout << "[CalculatorService] add(" << a << ", " << b << ") = " << result << std::endl;
        return CreateResult(result);
    }
    
    std::string HandleSubtract(const std::string& params) {
        double a, b;
        if (!ParseNumbers(params, a, b)) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        double result = a - b;
        std::cout << "[CalculatorService] subtract(" << a << ", " << b << ") = " << result << std::endl;
        return CreateResult(result);
    }
    
    std::string HandleMultiply(const std::string& params) {
        double a, b;
        if (!ParseNumbers(params, a, b)) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        double result = a * b;
        std::cout << "[CalculatorService] multiply(" << a << ", " << b << ") = " << result << std::endl;
        return CreateResult(result);
    }
    
    std::string HandleDivide(const std::string& params) {
        double a, b;
        if (!ParseNumbers(params, a, b)) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        if (b == 0) {
            return "{\"error\": \"Division by zero\"}";
        }
        
        double result = a / b;
        std::cout << "[CalculatorService] divide(" << a << ", " << b << ") = " << result << std::endl;
        return CreateResult(result);
    }
    
    void HandleAsyncAdd(const RpcContext& context, 
                       const std::string& params, 
                       RpcCallback callback) {
        // 模拟异步处理
        std::thread([this, params, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string result = HandleAdd(params);
            callback(RpcResult::SUCCESS, result);
        }).detach();
    }
};

/**
 * @brief 字符串处理服务实现
 */
class StringService : public RpcServiceBase
{
public:
    StringService() : RpcServiceBase("StringService")
    {
        SetServiceVersion("1.0.0");
        
        RegisterMethod("reverse", [this](const std::string& params) -> std::string {
            return HandleReverse(params);
        });
        
        RegisterMethod("uppercase", [this](const std::string& params) -> std::string {
            return HandleUppercase(params);
        });
        
        RegisterMethod("lowercase", [this](const std::string& params) -> std::string {
            return HandleLowercase(params);
        });
        
        std::cout << "[StringService] 服务初始化完成，注册了 " 
                  << GetMethodNames().size() << " 个方法" << std::endl;
    }
    
    ~StringService() override = default;

private:
    std::string ExtractString(const std::string& params) {
        // 简化的JSON解析，格式: {"text": "hello"}
        size_t textPos = params.find("\"text\":");
        if (textPos == std::string::npos) {
            return "";
        }
        
        size_t startQuote = params.find("\"", textPos + 7);
        if (startQuote == std::string::npos) {
            return "";
        }
        
        size_t endQuote = params.find("\"", startQuote + 1);
        if (endQuote == std::string::npos) {
            return "";
        }
        
        return params.substr(startQuote + 1, endQuote - startQuote - 1);
    }
    
    std::string CreateResult(const std::string& result) {
        return "{\"result\": \"" + result + "\"}";
    }
    
    std::string HandleReverse(const std::string& params) {
        std::string text = ExtractString(params);
        if (text.empty()) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        std::string reversed = text;
        std::reverse(reversed.begin(), reversed.end());
        
        std::cout << "[StringService] reverse(\"" << text << "\") = \"" << reversed << "\"" << std::endl;
        return CreateResult(reversed);
    }
    
    std::string HandleUppercase(const std::string& params) {
        std::string text = ExtractString(params);
        if (text.empty()) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        std::string upper = text;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        std::cout << "[StringService] uppercase(\"" << text << "\") = \"" << upper << "\"" << std::endl;
        return CreateResult(upper);
    }
    
    std::string HandleLowercase(const std::string& params) {
        std::string text = ExtractString(params);
        if (text.empty()) {
            return "{\"error\": \"Invalid parameters\"}";
        }
        
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        std::cout << "[StringService] lowercase(\"" << text << "\") = \"" << lower << "\"" << std::endl;
        return CreateResult(lower);
    }
};

/**
 * @brief RPC服务器示例
 */
class RpcServerExample
{
public:
         RpcServerExample() {
         // 初始化日志系统
         StructuredLoggerConfig logConfig;
         logConfig.MinLevel = StructuredLogLevel::INFO;
         StructuredLogger::Initialize(logConfig);
        
        // 创建RPC服务器
        RpcConfig config;
        config.DefaultTimeoutMs = 10000;
        config.MaxConcurrentCalls = 100;
        config.EnableMetrics = true;
        
        Server = std::make_unique<RpcServer>(config);
        
        // 注册服务
        auto calculatorService = std::make_shared<CalculatorService>();
        auto stringService = std::make_shared<StringService>();
        
        Server->RegisterService(calculatorService);
        Server->RegisterService(stringService);
        
        // 设置事件处理器
        Server->SetClientConnectedHandler([](const std::string& clientId) {
            std::cout << "[RpcServer] 客户端连接: " << clientId << std::endl;
        });
        
        Server->SetClientDisconnectedHandler([](const std::string& clientId) {
            std::cout << "[RpcServer] 客户端断开: " << clientId << std::endl;
        });
        
        Server->SetErrorHandler([](RpcResult result, const std::string& error) {
            std::cout << "[RpcServer] 错误: " << static_cast<int>(result) << " - " << error << std::endl;
        });
        
        // 添加中间件
        Server->AddMiddleware([](RpcContext& context) -> bool {
            std::cout << "[Middleware] 处理调用: " << context.ServiceName << "." << context.MethodName << std::endl;
            return true; // 允许继续处理
        });
    }
    
    void Start(const NetworkAddress& address) {
        std::cout << "[RpcServer] 启动RPC服务器在 " << address.ToString() << std::endl;
        
        RpcResult result = Server->Start(address);
        if (result != RpcResult::SUCCESS) {
            std::cout << "[RpcServer] 启动失败: " << static_cast<int>(result) << std::endl;
            return;
        }
        
        std::cout << "[RpcServer] 服务器启动成功" << std::endl;
        std::cout << "[RpcServer] 注册的服务: ";
        for (const auto& service : Server->GetRegisteredServices()) {
            std::cout << service << " ";
        }
        std::cout << std::endl;
    }
    
    void Stop() {
        if (Server) {
            Server->Stop();
            std::cout << "[RpcServer] 服务器已停止" << std::endl;
        }
    }
    
    void PrintStats() {
        if (Server) {
            auto stats = Server->GetStats();
            std::cout << "[RpcServer] 统计信息:" << std::endl;
            std::cout << "  总调用次数: " << stats.TotalCalls << std::endl;
            std::cout << "  成功调用: " << stats.SuccessfulCalls << std::endl;
            std::cout << "  失败调用: " << stats.FailedCalls << std::endl;
            std::cout << "  平均延迟: " << stats.AverageLatencyMs << "ms" << std::endl;
            std::cout << "  活跃调用: " << stats.ActiveCalls << std::endl;
        }
    }
    
    ~RpcServerExample() {
        Stop();
    }

private:
    std::unique_ptr<RpcServer> Server;
};

/**
 * @brief RPC客户端示例
 */
class RpcClientExample
{
public:
    RpcClientExample() {
        // 创建RPC客户端
        RpcConfig config;
        config.DefaultTimeoutMs = 5000;
        config.MaxRetries = 3;
        
        Client = std::make_unique<RpcClient>(config);
        
        // 设置事件处理器
        Client->SetConnectionStateHandler([](bool connected) {
            std::cout << "[RpcClient] 连接状态: " << (connected ? "已连接" : "已断开") << std::endl;
        });
        
        Client->SetErrorHandler([](RpcResult result, const std::string& error) {
            std::cout << "[RpcClient] 错误: " << static_cast<int>(result) << " - " << error << std::endl;
        });
    }
    
    bool Connect(const NetworkAddress& serverAddress) {
        std::cout << "[RpcClient] 连接到服务器 " << serverAddress.ToString() << std::endl;
        
        RpcResult result = Client->Connect(serverAddress);
        if (result != RpcResult::SUCCESS) {
            std::cout << "[RpcClient] 连接失败: " << static_cast<int>(result) << std::endl;
            return false;
        }
        
        std::cout << "[RpcClient] 连接成功" << std::endl;
        return true;
    }
    
    void Disconnect() {
        if (Client) {
            Client->Disconnect();
            std::cout << "[RpcClient] 已断开连接" << std::endl;
        }
    }
    
    void TestCalculatorService() {
        std::cout << "\n=== 测试计算器服务 ===" << std::endl;
        
        // 测试加法
        TestCall("CalculatorService", "add", "{\"a\": 10, \"b\": 20}");
        
        // 测试减法
        TestCall("CalculatorService", "subtract", "{\"a\": 50, \"b\": 30}");
        
        // 测试乘法
        TestCall("CalculatorService", "multiply", "{\"a\": 6, \"b\": 7}");
        
        // 测试除法
        TestCall("CalculatorService", "divide", "{\"a\": 100, \"b\": 5}");
        
        // 测试除零错误
        TestCall("CalculatorService", "divide", "{\"a\": 10, \"b\": 0}");
        
        // 测试异步加法
        TestAsyncCall("CalculatorService", "asyncAdd", "{\"a\": 15, \"b\": 25}");
    }
    
    void TestStringService() {
        std::cout << "\n=== 测试字符串服务 ===" << std::endl;
        
        // 测试字符串反转
        TestCall("StringService", "reverse", "{\"text\": \"hello world\"}");
        
        // 测试转大写
        TestCall("StringService", "uppercase", "{\"text\": \"hello world\"}");
        
        // 测试转小写
        TestCall("StringService", "lowercase", "{\"text\": \"HELLO WORLD\"}");
    }
    
         void TestBatchCalls() {
         std::cout << "\n=== 测试连续调用 ===" << std::endl;
         
         // 连续调用多个服务方法
         std::vector<std::pair<std::string, std::string>> testCalls = {
             {"CalculatorService", "add"},
             {"CalculatorService", "multiply"},
             {"StringService", "reverse"}
         };
         
         std::vector<std::string> testParams = {
             "{\"a\": 1, \"b\": 2}",
             "{\"a\": 3, \"b\": 4}",
             "{\"text\": \"test\"}"
         };
         
         for (size_t i = 0; i < testCalls.size(); ++i) {
             const auto& [serviceName, methodName] = testCalls[i];
             const std::string& params = testParams[i];
             
             std::string result;
             RpcResult callResult = Client->Call(serviceName, methodName, params, result, 5000);
             
             std::cout << "[RpcClient] 连续调用 " << (i + 1) << ": " 
                       << serviceName << "." << methodName 
                       << "(" << params << ") = " 
                       << static_cast<int>(callResult) << " - " << result << std::endl;
         }
     }
    
    void PrintStats() {
        if (Client) {
            auto stats = Client->GetStats();
            std::cout << "[RpcClient] 统计信息:" << std::endl;
            std::cout << "  总调用次数: " << stats.TotalCalls << std::endl;
            std::cout << "  成功调用: " << stats.SuccessfulCalls << std::endl;
            std::cout << "  失败调用: " << stats.FailedCalls << std::endl;
            std::cout << "  超时调用: " << stats.TimeoutCalls << std::endl;
            std::cout << "  平均延迟: " << stats.AverageLatencyMs << "ms" << std::endl;
        }
    }
    
    ~RpcClientExample() {
        Disconnect();
    }

private:
    std::unique_ptr<RpcClient> Client;
    
    void TestCall(const std::string& serviceName, 
                  const std::string& methodName, 
                  const std::string& params) {
        std::string result;
        RpcResult callResult = Client->Call(serviceName, methodName, params, result, 5000);
        
        std::cout << "[RpcClient] " << serviceName << "." << methodName 
                  << "(" << params << ") = " 
                  << static_cast<int>(callResult) << " - " << result << std::endl;
    }
    
    void TestAsyncCall(const std::string& serviceName, 
                       const std::string& methodName, 
                       const std::string& params) {
        RpcResult callResult = Client->CallAsync(serviceName, methodName, params, 
            [serviceName, methodName, params](RpcResult result, const std::string& response) {
                std::cout << "[RpcClient] 异步调用 " << serviceName << "." << methodName 
                          << "(" << params << ") = " 
                          << static_cast<int>(result) << " - " << response << std::endl;
            }, 5000);
        
        if (callResult != RpcResult::SUCCESS) {
            std::cout << "[RpcClient] 异步调用启动失败: " << static_cast<int>(callResult) << std::endl;
        }
    }
};

int main(int argc, char* argv[])
{
    std::cout << "=== Helianthus RPC 示例程序 ===" << std::endl;
    
    // 检查命令行参数
    bool runServer = false;
    bool runClient = false;
    std::string serverAddress = "127.0.0.1";
    uint16_t serverPort = 8080;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--server") {
            runServer = true;
        } else if (arg == "--client") {
            runClient = true;
        } else if (arg == "--address" && i + 1 < argc) {
            serverAddress = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            serverPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --server              运行RPC服务器" << std::endl;
            std::cout << "  --client              运行RPC客户端" << std::endl;
            std::cout << "  --address <地址>       服务器地址 (默认: 127.0.0.1)" << std::endl;
            std::cout << "  --port <端口>          服务器端口 (默认: 8080)" << std::endl;
            std::cout << "  --help, -h            显示此帮助信息" << std::endl;
            std::cout << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  " << argv[0] << " --server --port 8080" << std::endl;
            std::cout << "  " << argv[0] << " --client --address 127.0.0.1 --port 8080" << std::endl;
            return 0;
        }
    }
    
    // 如果没有指定模式，默认运行服务器
    if (!runServer && !runClient) {
        runServer = true;
    }
    
    NetworkAddress address(serverAddress, serverPort);
    
    if (runServer) {
        std::cout << "\n启动RPC服务器..." << std::endl;
        
        RpcServerExample server;
        server.Start(address);
        
        // 运行服务器一段时间
        std::cout << "服务器运行中，按 Ctrl+C 停止..." << std::endl;
        
        // 定期打印统计信息
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            server.PrintStats();
            
            // 运行30秒后自动停止（用于演示）
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() >= 30) {
                std::cout << "服务器运行30秒，自动停止..." << std::endl;
                break;
            }
        }
        
        server.Stop();
    }
    
    if (runClient) {
        std::cout << "\n启动RPC客户端..." << std::endl;
        
        RpcClientExample client;
        
        if (client.Connect(address)) {
            // 等待服务器准备就绪
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            // 测试各种服务
            client.TestCalculatorService();
            client.TestStringService();
            client.TestBatchCalls();
            
            // 打印统计信息
            client.PrintStats();
            
            // 等待异步调用完成
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
        
        client.Disconnect();
    }
    
    std::cout << "\n程序结束" << std::endl;
    return 0;
}
