#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <iostream>

#include "Types.h"

// 前向声明
namespace Helianthus::Common {
    class LogCategory;
}
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

namespace Helianthus::Common
{
    // 分类 Logger 类，封装单个分类的日志功能
    class CategoryLogger
    {
    public:
        explicit CategoryLogger(std::shared_ptr<spdlog::logger> LoggerPtr) 
            : LoggerInstance(LoggerPtr) {}

        // 分类日志方法
        template <typename... Args>
        void Log(spdlog::level::level_enum Level,
                 fmt::format_string<Args...> Format,
                 Args&&... Arguments)
        {
            if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            {
                LoggerInstance->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                   Level, Format, std::forward<Args>(Arguments)...);
            }
        }

        template <typename... Args>
        void Log(spdlog::level::level_enum Level,
                 const spdlog::source_loc& SourceLoc,
                 fmt::format_string<Args...> Format,
                 Args&&... Arguments)
        {
            if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            {
                LoggerInstance->log(SourceLoc, Level, Format, std::forward<Args>(Arguments)...);
            }
        }

        template <typename... Args>
        void Debug(fmt::format_string<Args...> Format, Args&&... Arguments)
        {
            Log(spdlog::level::debug, Format, std::forward<Args>(Arguments)...);
        }

        template <typename... Args>
        void Info(fmt::format_string<Args...> Format, Args&&... Arguments)
        {
            Log(spdlog::level::info, Format, std::forward<Args>(Arguments)...);
        }

        template <typename... Args>
        void Warn(fmt::format_string<Args...> Format, Args&&... Arguments)
        {
            Log(spdlog::level::warn, Format, std::forward<Args>(Arguments)...);
        }

        template <typename... Args>
        void Error(fmt::format_string<Args...> Format, Args&&... Arguments)
        {
            Log(spdlog::level::err, Format, std::forward<Args>(Arguments)...);
        }

        template <typename... Args>
        void Critical(fmt::format_string<Args...> Format, Args&&... Arguments)
        {
            Log(spdlog::level::critical, Format, std::forward<Args>(Arguments)...);
        }

        void Flush()
        {
            if (LoggerInstance)
            {
                LoggerInstance->flush();
            }
        }

    private:
        std::shared_ptr<spdlog::logger> LoggerInstance;
        static std::atomic<bool> ShuttingDownFlag;
    };

/**
 * @brief Centralized logging system using spdlog (transitional)
 *
 * This logger wraps spdlog functionality and provides a consistent
 * interface that will be replaced with a custom implementation later.
 */
class Logger
{
public:
    struct LoggerConfig
    {
        LogLevel Level;
        bool EnableConsole;
        bool EnableFile;
        std::string FilePath;
        size_t MaxFileSize;
        size_t MaxFiles;
        std::string Pattern;
        bool UseAsync;
        size_t QueueSize;
        int WorkerThreads;

        LoggerConfig()
            : Level(static_cast<LogLevel>(HELIANTHUS_DEFAULT_LOG_LEVEL)),
              EnableConsole(true),
              EnableFile(true),
              FilePath("logs/helianthus.log"),
              MaxFileSize(50 * 1024 * 1024),
              MaxFiles(5),
              Pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%P-%t-CID] [%n] [%s:%#] %v"),
              UseAsync(true),
              QueueSize(8192),
              WorkerThreads(1)
        {
        }
    };

    static void Initialize(const LoggerConfig& Config = LoggerConfig());
    static void Shutdown();
    static bool IsInitialized();
    static bool IsShuttingDown();

    // Category file configuration
    static void ConfigureCategoryFile(const std::string& CategoryName,
                                      const std::string& FilePath,
                                      size_t MaxFileSize = 50 * 1024 * 1024,
                                      size_t MaxFiles = 5);
    static void RemoveCategoryFile(const std::string& CategoryName);

    // Logging functions (string overloads)
    static void Debug(const std::string& Message);
    static void Info(const std::string& Message);
    static void Warn(const std::string& Message);
    static void Error(const std::string& Message);

    // Template logging with formatting (fmt-style)
    template <typename... Args>
    static void Debug(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            LoggerInstance->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                 spdlog::level::debug,
                                 Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Info(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            LoggerInstance->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                 spdlog::level::info,
                                 Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Warn(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            LoggerInstance->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                 spdlog::level::warn,
                                 Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Error(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance && !ShuttingDownFlag.load(std::memory_order_acquire))
            LoggerInstance->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                 spdlog::level::err,
                                 Format, std::forward<Args>(Arguments)...);
    }

    // Category logging API (called by macros) - 使用统一的 Logger 接口
    template <typename... Args>
    static void CategoryLog(const char* CategoryName,
                            spdlog::level::level_enum Level,
                            const spdlog::source_loc& SourceLoc,
                            fmt::format_string<Args...> Format,
                            Args&&... Arguments)
    {
        // 使用 Logger 类的统一管理，包括关闭检查
        if (ShuttingDownFlag.load(std::memory_order_acquire)) return;
 
        // 通过 Logger 类获取或创建分类 logger
        auto CategoryLogger = GetOrCreateCategory(CategoryName);
        // 使用 CategoryLogger 进行日志记录，传递源位置信息
        CategoryLogger.Log(Level, SourceLoc, Format, std::forward<Args>(Arguments)...);
    }

    // Configuration
    static void SetLevel(LogLevel Level);
    static LogLevel GetLevel();
    static void Flush();

private:
    static std::shared_ptr<spdlog::logger> LoggerInstance;
    static LoggerConfig CurrentConfig;
    static bool IsInitializedFlag;
    static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> CategoryLoggers;
    static std::atomic<bool> ShuttingDownFlag;

    // 获取或创建分类logger（控制台+可选分类文件 logs/<Category>.log）
    static CategoryLogger GetOrCreateCategory(const std::string& CategoryName);

    // 进程名缓存
    static std::string ProcessName;
    static std::string DetectProcessName();

    static spdlog::level::level_enum ConvertLogLevel(LogLevel Level);
    static LogLevel ConvertLogLevel(spdlog::level::level_enum Level);

    static void LoadCategoryFromEnv();
};

// Convenience macros for logging
#if HELIANTHUS_ENABLE_LOGGING
    #define HELIANTHUS_LOG_DEBUG(msg, ...) Helianthus::Common::Logger::Debug(msg, ##__VA_ARGS__)
    #define HELIANTHUS_LOG_INFO(msg, ...) Helianthus::Common::Logger::Info(msg, ##__VA_ARGS__)
    #define HELIANTHUS_LOG_WARN(msg, ...) Helianthus::Common::Logger::Warn(msg, ##__VA_ARGS__)
    #define HELIANTHUS_LOG_ERROR(msg, ...) Helianthus::Common::Logger::Error(msg, ##__VA_ARGS__)
#else
    #define HELIANTHUS_LOG_DEBUG(msg, ...)
    #define HELIANTHUS_LOG_INFO(msg, ...)
    #define HELIANTHUS_LOG_WARN(msg, ...)
    #define HELIANTHUS_LOG_ERROR(msg, ...)
#endif

}  // namespace Helianthus::Common
