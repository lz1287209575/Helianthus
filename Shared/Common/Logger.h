#pragma once

#include "Types.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

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
            
            LoggerConfig() 
                : Level(static_cast<LogLevel>(HELIANTHUS_DEFAULT_LOG_LEVEL))
                , EnableConsole(true)
                , EnableFile(true)
                , FilePath("logs/helianthus.log")
                , MaxFileSize(50 * 1024 * 1024) // 50MB
                , MaxFiles(5)
                , Pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v")
            {
            }
        };

        static void Initialize(const LoggerConfig& Config = LoggerConfig());
        static void Shutdown();
        static bool IsInitialized();

        // Logging functions
        static void Debug(const std::string& Message);
        static void Info(const std::string& Message);
        static void Warn(const std::string& Message);
        static void Error(const std::string& Message);

        // Template logging with formatting
        template<typename... Args>
        static void Debug(const std::string& Format, Args&&... Arguments)
        {
            if (LoggerInstance)
                LoggerInstance->debug(Format, std::forward<Args>(Arguments)...);
        }

        template<typename... Args>
        static void Info(const std::string& Format, Args&&... Arguments)
        {
            if (LoggerInstance)
                LoggerInstance->info(Format, std::forward<Args>(Arguments)...);
        }

        template<typename... Args>
        static void Warn(const std::string& Format, Args&&... Arguments)
        {
            if (LoggerInstance)
                LoggerInstance->warn(Format, std::forward<Args>(Arguments)...);
        }

        template<typename... Args>
        static void Error(const std::string& Format, Args&&... Arguments)
        {
            if (LoggerInstance)
                LoggerInstance->error(Format, std::forward<Args>(Arguments)...);
        }

        // Configuration
        static void SetLevel(LogLevel Level);
        static LogLevel GetLevel();
        static void Flush();

    private:
        static std::shared_ptr<spdlog::logger> LoggerInstance;
        static LoggerConfig CurrentConfig;
        static bool IsInitializedFlag;

        static spdlog::level::level_enum ConvertLogLevel(LogLevel Level);
        static LogLevel ConvertLogLevel(spdlog::level::level_enum Level);
    };

    // Convenience macros for logging
    #if HELIANTHUS_ENABLE_LOGGING
        #define HELIANTHUS_LOG_DEBUG(msg, ...) Helianthus::Common::Logger::Debug(msg, ##__VA_ARGS__)
        #define HELIANTHUS_LOG_INFO(msg, ...)  Helianthus::Common::Logger::Info(msg, ##__VA_ARGS__)
        #define HELIANTHUS_LOG_WARN(msg, ...)  Helianthus::Common::Logger::Warn(msg, ##__VA_ARGS__)
        #define HELIANTHUS_LOG_ERROR(msg, ...) Helianthus::Common::Logger::Error(msg, ##__VA_ARGS__)
    #else
        #define HELIANTHUS_LOG_DEBUG(msg, ...)
        #define HELIANTHUS_LOG_INFO(msg, ...)
        #define HELIANTHUS_LOG_WARN(msg, ...)
        #define HELIANTHUS_LOG_ERROR(msg, ...)
    #endif

} // namespace Helianthus::Common
