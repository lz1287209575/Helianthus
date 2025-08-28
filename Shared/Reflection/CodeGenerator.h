#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace Helianthus::Reflection::CodeGen
{
    // 属性标记
    enum class EPropertyFlags
    {
        None = 0,
        ScriptReadable = 1 << 0,
        ScriptWritable = 1 << 1,
        BlueprintReadOnly = 1 << 2,
        BlueprintReadWrite = 1 << 3,
        SaveGame = 1 << 4,
        Config = 1 << 5,
        EditAnywhere = 1 << 6,
        VisibleAnywhere = 1 << 7
    };
    
    // 函数标记
    enum class EFunctionFlags
    {
        None = 0,
        ScriptCallable = 1 << 0,
        ScriptEvent = 1 << 1,
        BlueprintCallable = 1 << 2,
        BlueprintEvent = 1 << 3,
        BlueprintPure = 1 << 4,
        NetMulticast = 1 << 5,
        NetServer = 1 << 6,
        NetClient = 1 << 7
    };
    
    // 类信息
    struct ClassInfo
    {
        std::string Name;
        std::string SuperClassName = "HObject";
        std::vector<std::string> Includes;
        std::vector<PropertyInfo> Properties;
        std::vector<FunctionInfo> Functions;
    };
    
    // 属性信息
    struct PropertyInfo
    {
        std::string Type;
        std::string Name;
        std::string DefaultValue;
        std::vector<std::string> Flags;
        std::string Description;
    };
    
    // 函数信息
    struct FunctionInfo
    {
        std::string ReturnType;
        std::string Name;
        std::vector<ParameterInfo> Parameters;
        std::vector<std::string> Flags;
        std::string Description;
        bool bIsConst = false;
    };
    
    // 参数信息
    struct ParameterInfo
    {
        std::string Type;
        std::string Name;
        std::string DefaultValue;
        bool bIsOutParam = false;
        bool bIsConst = false;
    };
    
    // 代码生成器
    class HCodeGenerator
    {
    public:
        static HCodeGenerator& Get();
        
        // 生成头文件
        bool GenerateHeader(const std::string& ClassName, const ClassInfo& Info);
        
        // 生成实现文件
        bool GenerateImplementation(const std::string& ClassName, const ClassInfo& Info);
        
        // 生成反射数据
        bool GenerateReflectionData(const std::string& ClassName, const ClassInfo& Info);
        
    private:
        std::string GenerateHeaderContent(const ClassInfo& Info);
        std::string GenerateImplementationContent(const ClassInfo& Info);
        std::string GenerateReflectionContent(const ClassInfo& Info);
        
        std::string GeneratePropertyRegistration(const PropertyInfo& Prop);
        std::string GenerateFunctionRegistration(const FunctionInfo& Func);
        
        std::string ToUpper(const std::string& Str);
        std::string ToMacro(const std::string& Str);
    };
}