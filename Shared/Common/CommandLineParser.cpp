#include "CommandLineParser.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Helianthus::Common
{
    void CommandLineParser::AddArgument(const std::string& ShortName, 
                                       const std::string& LongName,
                                       const std::string& Description,
                                       ArgType Type,
                                       bool Required,
                                       const std::string& DefaultValue)
    {
        Argument Arg;
        Arg.ShortName = ShortName;
        Arg.LongName = LongName;
        Arg.Description = Description;
        Arg.Type = Type;
        Arg.Required = Required;
        Arg.DefaultValue = DefaultValue;

        // 使用长名称作为键
        if (!LongName.empty())
        {
            Arguments[LongName] = Arg;
        }
        
        // 也使用短名称作为键（如果存在）
        if (!ShortName.empty())
        {
            Arguments[ShortName] = Arg;
        }
    }

    bool CommandLineParser::Parse(int argc, char* argv[])
    {
        std::vector<std::string> Args;
        for (int i = 1; i < argc; ++i) // 跳过程序名
        {
            Args.push_back(argv[i]);
        }
        return Parse(Args);
    }

    bool CommandLineParser::Parse(const std::vector<std::string>& Args)
    {
        Valid = false;
        LastError.clear();
        PositionalArgs.clear();

        // 重置所有参数值
        for (auto& [Key, Arg] : Arguments)
        {
            Arg.Values.clear();
        }

        for (size_t i = 0; i < Args.size(); ++i)
        {
            const std::string& Arg = Args[i];

            if (Arg.empty())
            {
                continue;
            }

            // 处理帮助标志
            if (Arg == "-h" || Arg == "--help")
            {
                ShowHelp("program");
                return false;
            }

            // 处理位置参数（不以-开头的参数）
            if (Arg[0] != '-')
            {
                PositionalArgs.push_back(Arg);
                continue;
            }

            // 处理选项参数
            std::string OptionName;
            std::string OptionValue;

            // 解析 --name=value 格式
            if (Arg.substr(0, 2) == "--")
            {
                size_t EqualPos = Arg.find('=');
                if (EqualPos != std::string::npos)
                {
                    OptionName = Arg.substr(2, EqualPos - 2);
                    OptionValue = Arg.substr(EqualPos + 1);
                }
                else
                {
                    OptionName = Arg.substr(2);
                }
            }
            // 解析 -n value 或 -nvalue 格式
            else if (Arg[0] == '-')
            {
                OptionName = Arg.substr(1);
                // 检查是否是 -nvalue 格式（单字符选项后面直接跟值）
                if (OptionName.length() > 1 && OptionName[1] != '-')
                {
                    // 检查是否是单字符选项
                    std::string SingleChar = OptionName.substr(0, 1);
                    auto SingleCharIt = Arguments.find(SingleChar);
                    if (SingleCharIt != Arguments.end())
                    {
                        // 是单字符选项，提取值
                        OptionValue = OptionName.substr(1);
                        OptionName = SingleChar;
                    }
                }
            }

            // 查找参数定义
            auto ArgIt = Arguments.find(OptionName);
            if (ArgIt == Arguments.end())
            {
                SetError("Unknown option: " + OptionName);
                return false;
            }

            Argument& ArgDef = ArgIt->second;

            // 处理不同类型的参数
            switch (ArgDef.Type)
            {
                case ArgType::FLAG:
                    ArgDef.Values.push_back("true");
                    break;

                case ArgType::STRING:
                case ArgType::INTEGER:
                case ArgType::FLOAT:
                case ArgType::MULTI:
                    if (OptionValue.empty())
                    {
                        // 需要下一个参数作为值
                        if (i + 1 >= Args.size())
                        {
                            SetError("Option " + OptionName + " requires a value");
                            return false;
                        }
                        OptionValue = Args[++i];
                    }
                    ArgDef.Values.push_back(OptionValue);
                    break;
            }

            // 同步更新对应的长名称或短名称参数
            if (OptionName == ArgDef.ShortName && !ArgDef.LongName.empty())
            {
                // 如果当前是短名称，同步更新长名称对应的参数
                auto LongNameIt = Arguments.find(ArgDef.LongName);
                if (LongNameIt != Arguments.end())
                {
                    LongNameIt->second.Values = ArgDef.Values;
                }
            }
            else if (OptionName == ArgDef.LongName && !ArgDef.ShortName.empty())
            {
                // 如果当前是长名称，同步更新短名称对应的参数
                auto ShortNameIt = Arguments.find(ArgDef.ShortName);
                if (ShortNameIt != Arguments.end())
                {
                    ShortNameIt->second.Values = ArgDef.Values;
                }
            }
        }

        // 验证必需参数
        if (!ValidateArguments())
        {
            return false;
        }

        Valid = true;
        return true;
    }

    bool CommandLineParser::HasFlag(const std::string& Name) const
    {
        auto Arg = FindArgument(Name);
        if (!Arg)
        {
            return false;
        }
        return !(*Arg)->Values.empty();
    }

    std::string CommandLineParser::GetString(const std::string& Name) const
    {
        auto Arg = FindArgument(Name);
        if (!Arg || (*Arg)->Values.empty())
        {
            return (*Arg)->DefaultValue;
        }
        return (*Arg)->Values[0];
    }

    int32_t CommandLineParser::GetInteger(const std::string& Name) const
    {
        std::string Value = GetString(Name);
        if (Value.empty())
        {
            return 0;
        }
        try
        {
            return std::stoi(Value);
        }
        catch (...)
        {
            return 0;
        }
    }

    float CommandLineParser::GetFloat(const std::string& Name) const
    {
        std::string Value = GetString(Name);
        if (Value.empty())
        {
            return 0.0f;
        }
        try
        {
            return std::stof(Value);
        }
        catch (...)
        {
            return 0.0f;
        }
    }

    std::vector<std::string> CommandLineParser::GetMulti(const std::string& Name) const
    {
        auto Arg = FindArgument(Name);
        if (!Arg)
        {
            return {};
        }
        return (*Arg)->Values;
    }

    std::vector<std::string> CommandLineParser::GetPositionalArgs() const
    {
        return PositionalArgs;
    }

    void CommandLineParser::ShowHelp(const std::string& ProgramName) const
    {
        std::cout << "Usage: " << ProgramName << " [OPTIONS] [ARGS...]\n\n";
        std::cout << "Options:\n";

        // 按长名称排序显示
        std::vector<std::string> SortedNames;
        for (const auto& [Key, Arg] : Arguments)
        {
            if (Key == Arg.LongName && !Arg.LongName.empty())
            {
                SortedNames.push_back(Key);
            }
        }
        std::sort(SortedNames.begin(), SortedNames.end());

        for (const auto& Name : SortedNames)
        {
            const Argument& Arg = Arguments.at(Name);
            
            std::string OptionStr;
            if (!Arg.ShortName.empty() && !Arg.LongName.empty())
            {
                OptionStr = "-" + Arg.ShortName + ", --" + Arg.LongName;
            }
            else if (!Arg.ShortName.empty())
            {
                OptionStr = "-" + Arg.ShortName;
            }
            else if (!Arg.LongName.empty())
            {
                OptionStr = "--" + Arg.LongName;
            }

            // 添加类型信息
            switch (Arg.Type)
            {
                case ArgType::FLAG:
                    break;
                case ArgType::STRING:
                    OptionStr += " <string>";
                    break;
                case ArgType::INTEGER:
                    OptionStr += " <integer>";
                    break;
                case ArgType::FLOAT:
                    OptionStr += " <float>";
                    break;
                case ArgType::MULTI:
                    OptionStr += " <value>";
                    break;
            }

            // 添加必需标记
            if (Arg.Required)
            {
                OptionStr += " (required)";
            }

            // 添加默认值
            if (!Arg.DefaultValue.empty() && Arg.Type != ArgType::FLAG)
            {
                OptionStr += " [default: " + Arg.DefaultValue + "]";
            }

            std::cout << std::setw(30) << std::left << OptionStr << " " << Arg.Description << "\n";
        }

        std::cout << "\n";
    }

    std::optional<const CommandLineParser::Argument*> CommandLineParser::FindArgument(const std::string& Name) const
    {
        auto It = Arguments.find(Name);
        if (It != Arguments.end())
        {
            return &It->second;
        }
        return std::nullopt;
    }

    std::optional<CommandLineParser::Argument*> CommandLineParser::FindArgument(const std::string& Name)
    {
        auto It = Arguments.find(Name);
        if (It != Arguments.end())
        {
            return &It->second;
        }
        return std::nullopt;
    }

    bool CommandLineParser::ValidateArguments()
    {
        for (const auto& [Key, Arg] : Arguments)
        {
            // 只检查长名称的参数，避免重复检查
            if (Key != Arg.LongName || Arg.LongName.empty())
            {
                continue;
            }

            if (Arg.Required && Arg.Values.empty() && Arg.DefaultValue.empty())
            {
                SetError("Required argument --" + Arg.LongName + " is missing");
                return false;
            }
        }
        return true;
    }

    void CommandLineParser::SetError(const std::string& Error)
    {
        LastError = Error;
        Valid = false;
    }

} // namespace Helianthus::Common
