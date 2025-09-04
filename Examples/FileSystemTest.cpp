#include "Shared/Common/LogCategories.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/Logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main()
{
    // 初始化日志系统
    Helianthus::Common::Logger::LoggerConfig logCfg;
    logCfg.Level = Helianthus::Common::LogLevel::INFO;
    logCfg.EnableConsole = true;
    logCfg.EnableFile = false;
    logCfg.UseAsync = false;
    Helianthus::Common::Logger::Initialize(logCfg);

    // 设置MQ分类的最小级别
    MQ.SetMinVerbosity(Helianthus::Common::LogVerbosity::VeryVerbose);

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 文件系统测试 ===");

    // 测试1：基本文件系统操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本文件系统操作");

    try
    {
        std::string testDir = "./test_filesystem_data";
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "检查目录: {}", testDir);

        std::filesystem::path dataDir(testDir);
        if (!std::filesystem::exists(dataDir))
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建目录: {}", testDir);
            std::filesystem::create_directories(dataDir);
        }

        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "目录操作成功");
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "目录操作失败: {}", e.what());
        return 1;
    }

    // 测试2：文件操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：文件操作");

    try
    {
        std::string testFile = "./test_filesystem_data/test.txt";
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建文件: {}", testFile);

        std::ofstream file(testFile);
        if (file.is_open())
        {
            file << "Hello, World!" << std::endl;
            file.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "文件创建成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "文件创建失败");
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "文件操作失败: {}", e.what());
        return 1;
    }

    // 测试3：二进制文件操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：二进制文件操作");

    try
    {
        std::string binaryFile = "./test_filesystem_data/test.bin";
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建二进制文件: {}", binaryFile);

        std::ofstream file(binaryFile, std::ios::binary | std::ios::out);
        if (file.is_open())
        {
            uint32_t testData = 12345;
            file.write(reinterpret_cast<const char*>(&testData), sizeof(testData));
            file.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "二进制文件创建成功");
        }
        else
        {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "二进制文件创建失败");
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "二进制文件操作失败: {}", e.what());
        return 1;
    }

    // 测试4：文件追加操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：文件追加操作");

    try
    {
        std::string appendFile = "./test_filesystem_data/append.txt";
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试文件追加: {}", appendFile);

        // 先创建文件
        {
            std::ofstream file(appendFile, std::ios::binary | std::ios::out);
            if (file.is_open())
            {
                file << "First line" << std::endl;
                file.close();
            }
        }

        // 然后追加内容
        {
            std::ofstream file(appendFile,
                               std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
            if (file.is_open())
            {
                file << "Second line" << std::endl;
                file.close();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "文件追加成功");
            }
            else
            {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "文件追加失败");
                return 1;
            }
        }
    }
    catch (const std::exception& e)
    {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "文件追加操作失败: {}", e.what());
        return 1;
    }

    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 文件系统测试完成 ===");

    return 0;
}
