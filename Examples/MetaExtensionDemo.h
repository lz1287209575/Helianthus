#pragma once

#include "Shared/RPC/IRpcServer.h"
#include "Shared/RPC/RpcReflection.h"
#include "Shared/Reflection/ReflectionMacros.h"
#include "Shared/Common/LogCategories.h"
#include <string>

// 示例：扩展元数据功能演示类（禁止自动注册，示例中手动注册）
HCLASS(MetaDemo,NoAutoRegister)
class MetaExtensionDemo : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    MetaExtensionDemo() : RpcServiceBase("MetaExtensionDemo") {}

    // 普通方法
    HMETHOD(Rpc) std::string GetName() const;
    
    // 纯函数示例 - 可在脚本或RPC对端调用的纯函数
    HMETHOD(PureFunction,Math) int Add(int a, int b) const noexcept;
    HMETHOD(PureFunction,Math) double Calculate(double x, double y) const;
    
    // 虚函数示例 - 通过标签标记
    HMETHOD(Virtual,Override) virtual std::string GetType() const;
    HMETHOD(Virtual,Override) virtual void Process() const noexcept;
    
    // 内联函数示例 - 通过标签标记
    HMETHOD(Inline,Utility) int GetValue() const { return 42; }
    HMETHOD(Inline,Utility) bool IsValid() const noexcept { return true; }
    
    // 已弃用函数示例 - 通过标签标记
    HMETHOD(Deprecated,Legacy) std::string OldMethod() const;
    
    // 复杂限定符示例 - 多个标签
    HMETHOD(Virtual,Override,Final,Const,Noexcept) virtual std::string ComplexMethod() const noexcept final;
    
    // 静态方法示例
    HMETHOD(Static,Utility) static std::string GetClassName();

    // HRPC_FACTORY()

private:
    std::string Name;
    int Value;
};

// 示例：纯函数工具类（禁止自动注册，示例中手动注册）
HCLASS(Utility,NoAutoRegister)
class PureFunctionUtils : public Helianthus::RPC::RpcServiceBase
{
    GENERATED_BODY()
public:
    PureFunctionUtils() : RpcServiceBase("PureFunctionUtils") {}
    // 纯函数工具方法 - 可在脚本或RPC对端调用的静态纯函数
    HMETHOD(PureFunction,Static,Math) static int Multiply(int a, int b) noexcept;
    HMETHOD(PureFunction,Static,Math) static double Power(double base, double exponent);
    HMETHOD(PureFunction,Static,String) static std::string Format(const std::string& format, int value);
    
    // 内联纯函数 - 可在脚本或RPC对端调用的内联函数
    HMETHOD(Inline,PureFunction,Math) static int Square(int x) noexcept { return x * x; }
    HMETHOD(Inline,PureFunction,Math) static bool IsEven(int x) noexcept { return x % 2 == 0; }
    
    // HRPC_FACTORY()
};
