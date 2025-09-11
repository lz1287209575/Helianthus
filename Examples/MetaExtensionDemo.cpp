#include "MetaExtensionDemo.h"
#include "generated/reflection_gen.h"
#include <iostream>
#include <memory>
#include <string>
#include <cmath>

// 实现 MetaExtensionDemo 的方法
std::string MetaExtensionDemo::GetName() const
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetName called");
    return "MetaExtensionDemo";
}

int MetaExtensionDemo::Add(int a, int b) const noexcept
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Add called with {} + {}", a, b);
    return a + b;
}

double MetaExtensionDemo::Calculate(double x, double y) const
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Calculate called with x={}, y={}", x, y);
    return std::sqrt(x * x + y * y);
}

std::string MetaExtensionDemo::GetType() const
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetType called");
    return "MetaExtensionDemo";
}

void MetaExtensionDemo::Process() const noexcept
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Process called");
}

std::string MetaExtensionDemo::OldMethod() const
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Warning, "OldMethod called - this method is deprecated");
    return "This is an old method";
}

std::string MetaExtensionDemo::ComplexMethod() const noexcept
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "ComplexMethod called");
    return "Complex method result";
}

std::string MetaExtensionDemo::GetClassName()
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "GetClassName called");
    return "MetaExtensionDemo";
}

// 实现 PureFunctionUtils 的方法
int PureFunctionUtils::Multiply(int a, int b) noexcept
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Multiply called with {} * {}", a, b);
    return a * b;
}

double PureFunctionUtils::Power(double base, double exponent)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Power called with base={}, exponent={}", base, exponent);
    return std::pow(base, exponent);
}

std::string PureFunctionUtils::Format(const std::string& format, int value)
{
    H_LOG(Rpc, Helianthus::Common::LogVerbosity::Log, "Format called with format='{}', value={}", format, value);
    return format + std::to_string(value);
}

int main()
{
    std::cout << "=== 反射元数据扩展演示 ===" << std::endl;
    std::cout << "演示通过标签系统实现的功能标记和限定符支持" << std::endl;
    
    // 创建RPC服务器
    auto Server = std::make_shared<Helianthus::RPC::RpcServer>();
    // 为本示例手动注册服务工厂，避免依赖全局生成库的工厂
    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService(
        "MetaExtensionDemo", "1.0.0",
        [](){ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<MetaExtensionDemo>()); }
    );
    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService(
        "PureFunctionUtils", "1.0.0",
        [](){ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<PureFunctionUtils>()); }
    );
    
    std::cout << "\n1. 挂载所有反射服务..." << std::endl;
    Helianthus::RPC::RegisterReflectedServices(*Server);
    
    std::cout << "\n2. 按标签筛选挂载（只挂载包含'Math'标签的方法）..." << std::endl;
    std::cout << "   PureFunction 标签表示可在脚本或RPC对端调用的纯函数" << std::endl;
    std::vector<std::string> MathTags = {"Math"};
    Helianthus::RPC::RegisterReflectedServices(*Server, MathTags);
    
    std::cout << "\n3. 按标签筛选挂载（只挂载包含'Utility'标签的方法）..." << std::endl;
    std::cout << "   Inline 标签表示内联函数，Static 标签表示静态方法" << std::endl;
    std::vector<std::string> UtilityTags = {"Utility"};
    Helianthus::RPC::RegisterReflectedServices(*Server, UtilityTags);
    
    std::cout << "\n4. 按标签筛选挂载（只挂载包含'PureFunction'标签的方法）..." << std::endl;
    std::cout << "   这些方法可以在脚本或RPC对端直接调用" << std::endl;
    std::vector<std::string> PureFunctionTags = {"PureFunction"};
    Helianthus::RPC::RegisterReflectedServices(*Server, PureFunctionTags);
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    std::cout << "支持的标签类型：" << std::endl;
    std::cout << "  - PureFunction: 可在脚本或RPC对端调用的纯函数" << std::endl;
    std::cout << "  - Virtual: 虚函数" << std::endl;
    std::cout << "  - Inline: 内联函数" << std::endl;
    std::cout << "  - Deprecated: 已弃用函数" << std::endl;
    std::cout << "  - Static: 静态方法" << std::endl;
    std::cout << "  - Const: const 方法" << std::endl;
    std::cout << "  - Noexcept: noexcept 方法" << std::endl;
    std::cout << "  - Override: override 方法" << std::endl;
    std::cout << "  - Final: final 方法" << std::endl;
    return 0;
}
