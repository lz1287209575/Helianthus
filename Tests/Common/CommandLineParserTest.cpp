#include "Shared/Common/CommandLineParser.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Helianthus::Common;

class CommandLineParserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 设置测试参数
        Parser.AddArgument("c", "config", "配置文件路径", CommandLineParser::ArgType::STRING, false, "default.json");
        Parser.AddArgument("v", "verbose", "详细输出", CommandLineParser::ArgType::FLAG);
        Parser.AddArgument("p", "port", "端口号", CommandLineParser::ArgType::INTEGER, false, "8080");
        Parser.AddArgument("t", "timeout", "超时时间", CommandLineParser::ArgType::FLOAT, false, "1.0");
        Parser.AddArgument("f", "files", "文件列表", CommandLineParser::ArgType::MULTI);
    }

    CommandLineParser Parser;
};

TEST_F(CommandLineParserTest, BasicParsing)
{
    std::vector<std::string> Args = {"--config", "test.json", "--verbose"};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    EXPECT_EQ(Parser.GetString("config"), "test.json");
    EXPECT_TRUE(Parser.HasFlag("verbose"));
    EXPECT_EQ(Parser.GetInteger("port"), 8080); // 默认值
}

TEST_F(CommandLineParserTest, ShortOptions)
{
    std::vector<std::string> Args = {"-c", "test.json", "-v"};
    
    bool ParseResult = Parser.Parse(Args);
    EXPECT_TRUE(ParseResult);
    EXPECT_TRUE(Parser.IsValid());
    
    std::string ConfigValue = Parser.GetString("config");
    bool VerboseFlag = Parser.HasFlag("verbose");
    
    EXPECT_EQ(ConfigValue, "test.json");
    EXPECT_TRUE(VerboseFlag);
}

TEST_F(CommandLineParserTest, MixedOptions)
{
    std::vector<std::string> Args = {"--config=test.json", "-v", "--port", "9090"};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    EXPECT_EQ(Parser.GetString("config"), "test.json");
    EXPECT_TRUE(Parser.HasFlag("verbose"));
    EXPECT_EQ(Parser.GetInteger("port"), 9090);
}

TEST_F(CommandLineParserTest, PositionalArgs)
{
    std::vector<std::string> Args = {"--config", "test.json", "arg1", "arg2"};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    
    auto PositionalArgs = Parser.GetPositionalArgs();
    EXPECT_EQ(PositionalArgs.size(), 2);
    EXPECT_EQ(PositionalArgs[0], "arg1");
    EXPECT_EQ(PositionalArgs[1], "arg2");
}

TEST_F(CommandLineParserTest, MultiValueArgs)
{
    std::vector<std::string> Args = {"--files", "file1.txt", "--files", "file2.txt"};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    
    auto Files = Parser.GetMulti("files");
    EXPECT_EQ(Files.size(), 2);
    EXPECT_EQ(Files[0], "file1.txt");
    EXPECT_EQ(Files[1], "file2.txt");
}

TEST_F(CommandLineParserTest, FloatValues)
{
    std::vector<std::string> Args = {"--timeout", "2.5"};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    EXPECT_FLOAT_EQ(Parser.GetFloat("timeout"), 2.5f);
}

TEST_F(CommandLineParserTest, DefaultValues)
{
    std::vector<std::string> Args = {};
    
    EXPECT_TRUE(Parser.Parse(Args));
    EXPECT_TRUE(Parser.IsValid());
    EXPECT_EQ(Parser.GetString("config"), "default.json");
    EXPECT_EQ(Parser.GetInteger("port"), 8080);
    EXPECT_FLOAT_EQ(Parser.GetFloat("timeout"), 1.0f);
}

TEST_F(CommandLineParserTest, UnknownOption)
{
    std::vector<std::string> Args = {"--unknown", "value"};
    
    EXPECT_FALSE(Parser.Parse(Args));
    EXPECT_FALSE(Parser.IsValid());
    EXPECT_FALSE(Parser.GetLastError().empty());
    EXPECT_TRUE(Parser.GetLastError().find("Unknown option") != std::string::npos);
}

TEST_F(CommandLineParserTest, MissingValue)
{
    std::vector<std::string> Args = {"--config"};
    
    EXPECT_FALSE(Parser.Parse(Args));
    EXPECT_FALSE(Parser.IsValid());
    EXPECT_FALSE(Parser.GetLastError().empty());
    EXPECT_TRUE(Parser.GetLastError().find("requires a value") != std::string::npos);
}

TEST_F(CommandLineParserTest, HelpFlag)
{
    std::vector<std::string> Args = {"--help"};
    
    EXPECT_FALSE(Parser.Parse(Args)); // 帮助标志返回false但不设置错误
    EXPECT_TRUE(Parser.GetLastError().empty());
}

TEST_F(CommandLineParserTest, ShortHelpFlag)
{
    std::vector<std::string> Args = {"-h"};
    
    EXPECT_FALSE(Parser.Parse(Args)); // 帮助标志返回false但不设置错误
    EXPECT_TRUE(Parser.GetLastError().empty());
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
