#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <iostream>

namespace Helianthus::Common
{
    /**
     * @brief 命令行参数解析器
     * 
     * 支持以下格式的命令行参数：
     * - 短选项：-h, -v, -f file.txt
     * - 长选项：--help, --version, --file=file.txt, --file file.txt
     * - 位置参数：program arg1 arg2
     * - 布尔标志：--verbose, --quiet
     */
    class CommandLineParser
    {
    public:
        // 参数类型
        enum class ArgType
        {
            FLAG,       // 布尔标志，如 --help
            STRING,     // 字符串参数，如 --file=value
            INTEGER,    // 整数参数，如 --port=8080
            FLOAT,      // 浮点数参数，如 --timeout=1.5
            MULTI       // 多值参数，如 --include=file1 --include=file2
        };

        // 参数定义
        struct Argument
        {
            std::string ShortName;      // 短名称，如 "h"
            std::string LongName;       // 长名称，如 "help"
            std::string Description;    // 描述
            ArgType Type;               // 参数类型
            bool Required;              // 是否必需
            std::string DefaultValue;   // 默认值
            std::vector<std::string> Values; // 实际值（支持多值）
        };

        CommandLineParser() = default;
        ~CommandLineParser() = default;

        // 添加参数定义
        void AddArgument(const std::string& ShortName, 
                        const std::string& LongName,
                        const std::string& Description,
                        ArgType Type = ArgType::STRING,
                        bool Required = false,
                        const std::string& DefaultValue = "");

        // 解析命令行参数
        bool Parse(int argc, char* argv[]);
        bool Parse(const std::vector<std::string>& Args);

        // 获取参数值
        bool HasFlag(const std::string& Name) const;
        std::string GetString(const std::string& Name) const;
        int32_t GetInteger(const std::string& Name) const;
        float GetFloat(const std::string& Name) const;
        std::vector<std::string> GetMulti(const std::string& Name) const;

        // 获取位置参数
        std::vector<std::string> GetPositionalArgs() const;

        // 显示帮助信息
        void ShowHelp(const std::string& ProgramName) const;

        // 获取错误信息
        std::string GetLastError() const { return LastError; }

        // 检查是否有效
        bool IsValid() const { return Valid; }

    private:
        std::map<std::string, Argument> Arguments;
        std::vector<std::string> PositionalArgs;
        std::string LastError;
        bool Valid = false;

        // 内部辅助方法
        std::optional<const Argument*> FindArgument(const std::string& Name) const;
        std::optional<Argument*> FindArgument(const std::string& Name);
        bool ValidateArguments();
        void SetError(const std::string& Error);
    };

    // 便利宏定义
    #define HELIANTHUS_CLI_FLAG(parser, short_name, long_name, desc) \
        parser.AddArgument(short_name, long_name, desc, CommandLineParser::ArgType::FLAG)

    #define HELIANTHUS_CLI_STRING(parser, short_name, long_name, desc, required, default_val) \
        parser.AddArgument(short_name, long_name, desc, CommandLineParser::ArgType::STRING, required, default_val)

    #define HELIANTHUS_CLI_INTEGER(parser, short_name, long_name, desc, required, default_val) \
        parser.AddArgument(short_name, long_name, desc, CommandLineParser::ArgType::INTEGER, required, default_val)

    #define HELIANTHUS_CLI_FLOAT(parser, short_name, long_name, desc, required, default_val) \
        parser.AddArgument(short_name, long_name, desc, CommandLineParser::ArgType::FLOAT, required, default_val)

    #define HELIANTHUS_CLI_MULTI(parser, short_name, long_name, desc, required) \
        parser.AddArgument(short_name, long_name, desc, CommandLineParser::ArgType::MULTI, required)

} // namespace Helianthus::Common
