#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

#include "Shared/Common/Logger.h"
#include "Shared/Common/LogCategory.h"
#include "Shared/Common/LogCategories.h"

// 模拟FileBasedPersistence的文件流结构
class MockFileBasedPersistence {
private:
    std::fstream QueueDataFile;
    std::fstream MessageDataFile;
    std::fstream IndexFile;
    std::string DataDirectory;
    
public:
    MockFileBasedPersistence() {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence构造函数");
    }
    
    ~MockFileBasedPersistence() {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构函数开始");
        
        // 关闭文件流
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始关闭文件流");
        
        if (QueueDataFile.is_open()) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭QueueDataFile");
            QueueDataFile.close();
        }
        
        if (MessageDataFile.is_open()) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭MessageDataFile");
            MessageDataFile.close();
        }
        
        if (IndexFile.is_open()) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "关闭IndexFile");
            IndexFile.close();
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "文件流关闭完成");
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构函数完成");
    }
    
    bool Initialize(const std::string& dataDir) {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始初始化文件流");
        
        DataDirectory = dataDir;
        
        // 创建数据目录
        try {
            std::filesystem::create_directories(DataDirectory);
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "数据目录创建成功: {}", DataDirectory);
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "数据目录创建失败: {}", e.what());
            return false;
        }
        
        // 打开文件流
        try {
            std::string queueDataPath = DataDirectory + "/queue_data.bin";
            std::string messageDataPath = DataDirectory + "/messages.bin";
            std::string indexPath = DataDirectory + "/index.bin";
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "打开QueueDataFile: {}", queueDataPath);
            QueueDataFile.open(queueDataPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!QueueDataFile.is_open()) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "QueueDataFile打开失败");
                return false;
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "打开MessageDataFile: {}", messageDataPath);
            MessageDataFile.open(messageDataPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!MessageDataFile.is_open()) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "MessageDataFile打开失败");
                return false;
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "打开IndexFile: {}", indexPath);
            IndexFile.open(indexPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!IndexFile.is_open()) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "IndexFile打开失败");
                return false;
            }
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "所有文件流打开成功");
            return true;
            
        } catch (const std::exception& e) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "文件流打开异常: {}", e.what());
            return false;
        }
    }
    
    void TestOperation() {
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "执行文件流测试操作");
        
        if (QueueDataFile.is_open()) {
            QueueDataFile.seekp(0, std::ios::end);
            QueueDataFile.write("test", 4);
            QueueDataFile.flush();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "QueueDataFile写入测试数据");
        }
        
        if (MessageDataFile.is_open()) {
            MessageDataFile.seekp(0, std::ios::end);
            MessageDataFile.write("test", 4);
            MessageDataFile.flush();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MessageDataFile写入测试数据");
        }
        
        if (IndexFile.is_open()) {
            IndexFile.seekp(0, std::ios::end);
            IndexFile.write("test", 4);
            IndexFile.flush();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "IndexFile写入测试数据");
        }
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "文件流测试操作完成");
    }
};

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
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== Helianthus 文件流测试 ===");
    
    // 测试1：基本文件流操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试1：基本文件流操作");
    {
        std::fstream testFile;
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "创建testFile");
        
        testFile.open("./test_filestream_data/test.bin", std::ios::binary | std::ios::out | std::ios::trunc);
        if (testFile.is_open()) {
            testFile.write("test", 4);
            testFile.close();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "testFile操作完成");
        }
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "testFile析构完成");
    
    // 测试2：MockFileBasedPersistence文件流操作
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试2：MockFileBasedPersistence文件流操作");
    {
        auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence创建成功");
        
        if (mockPersistence->Initialize("./test_filestream_data")) {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence初始化成功");
            
            mockPersistence->TestOperation();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试操作完成");
            
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "开始析构MockFileBasedPersistence");
            mockPersistence.reset();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "MockFileBasedPersistence析构完成");
        } else {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "MockFileBasedPersistence初始化失败");
        }
    }
    
    // 测试3：在独立线程中操作文件流
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试3：在独立线程中操作文件流");
    {
        std::thread fileStreamThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始创建MockFileBasedPersistence");
            auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence创建成功");
            
            if (mockPersistence->Initialize("./test_filestream_thread_data")) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence初始化成功");
                
                mockPersistence->TestOperation();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：测试操作完成");
                
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始析构MockFileBasedPersistence");
                mockPersistence.reset();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence析构完成");
            } else {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "线程内：MockFileBasedPersistence初始化失败");
            }
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待文件流线程完成");
        fileStreamThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "文件流线程完成");
    }
    
    // 测试4：模拟FileBasedPersistence的完整生命周期
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "测试4：模拟FileBasedPersistence的完整生命周期");
    {
        std::thread lifecycleThread([]() {
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始生命周期测试");
            
            auto mockPersistence = std::make_unique<MockFileBasedPersistence>();
            H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence创建成功");
            
            if (mockPersistence->Initialize("./test_filestream_lifecycle_data")) {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence初始化成功");
                
                // 模拟多次操作
                for (int i = 0; i < 5; ++i) {
                    mockPersistence->TestOperation();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：开始析构MockFileBasedPersistence");
                mockPersistence.reset();
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：MockFileBasedPersistence析构完成");
                
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "线程内：生命周期测试完成");
            } else {
                H_LOG(MQ, Helianthus::Common::LogVerbosity::Error, "线程内：MockFileBasedPersistence初始化失败");
            }
        });
        
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "等待生命周期线程完成");
        lifecycleThread.join();
        H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "生命周期线程完成");
    }
    
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display, "=== 文件流测试完成 ===");
    
    return 0;
}
