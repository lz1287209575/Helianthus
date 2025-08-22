#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Types.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

namespace Helianthus::Common
{
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
              Pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v"),
              UseAsync(true),
              QueueSize(8192),
              WorkerThreads(1)
        {
        }
    };

    static void Initialize(const LoggerConfig& Config = LoggerConfig());
    static void Shutdown();
    static bool IsInitialized();

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
        if (LoggerInstance)
            LoggerInstance->debug(Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Info(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance)
            LoggerInstance->info(Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Warn(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance)
            LoggerInstance->warn(Format, std::forward<Args>(Arguments)...);
    }

    template <typename... Args>
    static void Error(fmt::format_string<Args...> Format, Args&&... Arguments)
    {
        if (LoggerInstance)
            LoggerInstance->error(Format, std::forward<Args>(Arguments)...);
    }

    // Category logging API (called by macros)
    template <typename... Args>
    static void CategoryLog(const char* CategoryName,
                            spdlog::level::level_enum Level,
                            fmt::format_string<Args...> Format,
                            Args&&... Arguments)
    {
        auto It = CategoryLoggers.find(CategoryName);
        if (It != CategoryLoggers.end() && It->second)
        {
            It->second->log(Level, Format, std::forward<Args>(Arguments)...);
        }
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
