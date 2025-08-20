#include "Shared/Common/Logger.h"

#include <vector>

// Avoid Windows DEBUG/ERROR macro collisions with enum LogLevel members
#ifdef DEBUG
    #undef DEBUG
#endif
#ifdef ERROR
    #undef ERROR
#endif

namespace Helianthus::Common
{
std::shared_ptr<spdlog::logger> Logger::LoggerInstance;
Logger::LoggerConfig Logger::CurrentConfig;
bool Logger::IsInitializedFlag = false;

spdlog::level::level_enum Logger::ConvertLogLevel(LogLevel Level)
{
    switch (Level)
    {
        case LogLevel::DEBUG:
        {
            return spdlog::level::debug;
        }
        case LogLevel::INFO:
        {
            return spdlog::level::info;
        }
        case LogLevel::WARN:
        {
            return spdlog::level::warn;
        }
        case LogLevel::ERROR:
        {
            return spdlog::level::err;
        }
        default:
        {
            return spdlog::level::info;
        }
    }
}

LogLevel Logger::ConvertLogLevel(spdlog::level::level_enum Level)
{
    switch (Level)
    {
        case spdlog::level::debug:
        {
            return LogLevel::DEBUG;
        }
        case spdlog::level::info:
        {
            return LogLevel::INFO;
        }
        case spdlog::level::warn:
        {
            return LogLevel::WARN;
        }
        case spdlog::level::err:
        {
            return LogLevel::ERROR;
        }
        default:
        {
            return LogLevel::INFO;
        }
    }
}

void Logger::Initialize(const LoggerConfig& Config)
{
    if (IsInitializedFlag)
        return;
    CurrentConfig = Config;
    std::vector<spdlog::sink_ptr> Sinks;
    if (Config.EnableConsole)
    {
        Sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (Config.EnableFile)
    {
        Sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            Config.FilePath, Config.MaxFileSize, Config.MaxFiles));
    }
    LoggerInstance = std::make_shared<spdlog::logger>("helianthus", Sinks.begin(), Sinks.end());
    LoggerInstance->set_level(ConvertLogLevel(Config.Level));
    LoggerInstance->set_pattern(Config.Pattern);
    IsInitializedFlag = true;
}

void Logger::Shutdown()
{
    LoggerInstance.reset();
    IsInitializedFlag = false;
}

bool Logger::IsInitialized()
{
    return IsInitializedFlag;
}

void Logger::Debug(const std::string& Message)
{
    if (LoggerInstance)
        LoggerInstance->debug("{}", Message);
}

void Logger::Info(const std::string& Message)
{
    if (LoggerInstance)
        LoggerInstance->info("{}", Message);
}

void Logger::Warn(const std::string& Message)
{
    if (LoggerInstance)
        LoggerInstance->warn("{}", Message);
}

void Logger::Error(const std::string& Message)
{
    if (LoggerInstance)
        LoggerInstance->error("{}", Message);
}

void Logger::SetLevel(LogLevel Level)
{
    if (LoggerInstance)
        LoggerInstance->set_level(ConvertLogLevel(Level));
}

LogLevel Logger::GetLevel()
{
    if (LoggerInstance)
        return ConvertLogLevel(LoggerInstance->level());
    return CurrentConfig.Level;
}

void Logger::Flush()
{
    if (LoggerInstance)
        LoggerInstance->flush();
}
}  // namespace Helianthus::Common
