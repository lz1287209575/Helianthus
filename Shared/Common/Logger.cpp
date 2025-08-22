#include "Shared/Common/Logger.h"

#include <vector>
#include <cstdlib>
#include <regex>

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
std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> Logger::CategoryLoggers;

static std::string Trim(const std::string& S)
{
    auto Start = S.find_first_not_of(" \t\n\r");
    auto End = S.find_last_not_of(" \t\n\r");
    if (Start == std::string::npos) return "";
    return S.substr(Start, End - Start + 1);
}

spdlog::level::level_enum Logger::ConvertLogLevel(LogLevel Level)
{
    switch (Level)
    {
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARN: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        default: return spdlog::level::info;
    }
}

LogLevel Logger::ConvertLogLevel(spdlog::level::level_enum Level)
{
    switch (Level)
    {
        case spdlog::level::debug: return LogLevel::DEBUG;
        case spdlog::level::info: return LogLevel::INFO;
        case spdlog::level::warn: return LogLevel::WARN;
        case spdlog::level::err: return LogLevel::ERROR;
        default: return LogLevel::INFO;
    }
}

void Logger::Initialize(const LoggerConfig& Config)
{
    if (IsInitializedFlag)
        return;
    CurrentConfig = Config;

    // async thread pool
    if (Config.UseAsync)
    {
        spdlog::init_thread_pool(Config.QueueSize, Config.WorkerThreads);
    }

    std::vector<spdlog::sink_ptr> Sinks;
    if (Config.EnableConsole)
    {
        auto ConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        Sinks.push_back(ConsoleSink);
    }
    if (Config.EnableFile)
    {
        auto FileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            Config.FilePath, Config.MaxFileSize, Config.MaxFiles);
        Sinks.push_back(FileSink);
    }

    if (Config.UseAsync)
    {
        LoggerInstance = std::make_shared<spdlog::async_logger>(
            "helianthus", begin(Sinks), end(Sinks), spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);
        spdlog::register_logger(LoggerInstance);
    }
    else
    {
        LoggerInstance = std::make_shared<spdlog::logger>("helianthus", begin(Sinks), end(Sinks));
        spdlog::register_logger(LoggerInstance);
    }

    LoggerInstance->set_level(ConvertLogLevel(Config.Level));
    LoggerInstance->set_pattern(Config.Pattern);

    LoadCategoryFromEnv();

    IsInitializedFlag = true;
}

void Logger::Shutdown()
{
    for (auto& [Name, CatLogger] : CategoryLoggers)
    {
        if (CatLogger)
        {
            CatLogger->flush();
            spdlog::drop(CatLogger->name());
        }
    }
    CategoryLoggers.clear();

    if (LoggerInstance)
    {
        LoggerInstance->flush();
        spdlog::drop(LoggerInstance->name());
        LoggerInstance.reset();
    }
    if (CurrentConfig.UseAsync)
    {
        spdlog::shutdown();
    }
    IsInitializedFlag = false;
}

bool Logger::IsInitialized()
{
    return IsInitializedFlag;
}

void Logger::ConfigureCategoryFile(const std::string& CategoryName,
                                   const std::string& FilePath,
                                   size_t MaxFileSize,
                                   size_t MaxFiles)
{
    std::vector<spdlog::sink_ptr> Sinks;
    if (CurrentConfig.EnableConsole)
    {
        Sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    Sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(FilePath, MaxFileSize, MaxFiles));

    std::shared_ptr<spdlog::logger> CatLogger;
    if (CurrentConfig.UseAsync)
    {
        CatLogger = std::make_shared<spdlog::async_logger>(
            std::string("cat_") + CategoryName, begin(Sinks), end(Sinks), spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);
    }
    else
    {
        CatLogger = std::make_shared<spdlog::logger>(std::string("cat_") + CategoryName, begin(Sinks), end(Sinks));
    }
    CatLogger->set_level(ConvertLogLevel(CurrentConfig.Level));
    CatLogger->set_pattern(CurrentConfig.Pattern);
    spdlog::register_logger(CatLogger);

    CategoryLoggers[CategoryName] = std::move(CatLogger);
}

void Logger::RemoveCategoryFile(const std::string& CategoryName)
{
    auto It = CategoryLoggers.find(CategoryName);
    if (It != CategoryLoggers.end() && It->second)
    {
        It->second->flush();
        spdlog::drop(It->second->name());
        CategoryLoggers.erase(It);
    }
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

void Logger::LoadCategoryFromEnv()
{
    // 约定：H_LOG_CATEGORY_<NAME>=/path/to/file.log（逗号可分隔多个配置）
    // 例：H_LOG_CATEGORY_Net=logs/net.log;H_LOG_CATEGORY_Perf=logs/perf.log
    const char* Env = std::getenv("H_LOG_CATEGORY");
    if (!Env) return;
    std::string All = Env;

    // 支持分号或逗号分隔： Net=path;Perf=path2
    std::regex PairRe("\\s*([A-Za-z0-9_]+)\\s*=\\s*([^;,]+)\\s*");
    auto Begin = std::sregex_iterator(All.begin(), All.end(), PairRe);
    auto End = std::sregex_iterator();
    for (auto It = Begin; It != End; ++It)
    {
        std::smatch M = *It;
        if (M.size() >= 3)
        {
            auto Name = Trim(M[1].str());
            auto Path = Trim(M[2].str());
            if (!Name.empty() && !Path.empty())
            {
                ConfigureCategoryFile(Name, Path, CurrentConfig.MaxFileSize, CurrentConfig.MaxFiles);
            }
        }
    }
}
}  // namespace Helianthus::Common
