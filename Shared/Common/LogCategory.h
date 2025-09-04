#pragma once

#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Common/Logger.h"
#include "Common/Types.h"

namespace Helianthus::Common
{
enum class LogVerbosity
{
    Fatal = 0,
    Error,
    Warning,
    Display,
    Log,
    Verbose,
    VeryVerbose,
};

class LogCategory
{
public:
    // 单例获取方法
    static LogCategory& GetInstance(const std::string& CategoryName,
                                    LogVerbosity DefaultVerbosity = LogVerbosity::Log)
    {
        static std::mutex InstanceMutex;
        static std::unordered_map<std::string, std::unique_ptr<LogCategory>> Instances;

        std::lock_guard<std::mutex> Lock(InstanceMutex);

        auto It = Instances.find(CategoryName);
        if (It != Instances.end())
        {
            return *(It->second);
        }

        // 创建新实例
        auto Instance =
            std::unique_ptr<LogCategory>(new LogCategory(CategoryName, DefaultVerbosity));
        auto& Ref = *Instance;
        Instances[CategoryName] = std::move(Instance);

        return Ref;
    }

    const char* GetName() const
    {
        return Name.c_str();
    }

    void SetMinVerbosity(LogVerbosity Verbosity)
    {
        MinVerbosity.store(Verbosity, std::memory_order_relaxed);
    }
    LogVerbosity GetMinVerbosity() const
    {
        return MinVerbosity.load(std::memory_order_relaxed);
    }

    bool IsLoggable(LogVerbosity Verbosity) const
    {
        return static_cast<int>(Verbosity) >=
               static_cast<int>(MinVerbosity.load(std::memory_order_relaxed));
    }

    // 兼容性方法
    static void SetCategoryMinVerbosity(const std::string& CategoryName, LogVerbosity Verbosity)
    {
        GetInstance(CategoryName).SetMinVerbosity(Verbosity);
    }

private:
    // 私有构造函数，防止外部创建实例
    explicit LogCategory(const std::string& NameIn,
                         LogVerbosity DefaultVerbosity = LogVerbosity::Log)
        : Name(NameIn), MinVerbosity(DefaultVerbosity)
    {
    }

    // 禁用拷贝和赋值
    LogCategory(const LogCategory&) = delete;
    LogCategory& operator=(const LogCategory&) = delete;

    std::string Name;
    mutable std::atomic<LogVerbosity> MinVerbosity;
};

inline spdlog::level::level_enum ToSpdLevel(LogVerbosity Verbosity)
{
    switch (Verbosity)
    {
        case LogVerbosity::Fatal:
            return spdlog::level::critical;
        case LogVerbosity::Error:
            return spdlog::level::err;
        case LogVerbosity::Warning:
            return spdlog::level::warn;
        case LogVerbosity::Display:
            return spdlog::level::info;
        case LogVerbosity::Log:
            return spdlog::level::info;
        case LogVerbosity::Verbose:
            return spdlog::level::debug;
        case LogVerbosity::VeryVerbose:
            return spdlog::level::trace;
    }
    return spdlog::level::info;
}
}  // namespace Helianthus::Common

// 宏接口
#define H_DECLARE_LOG_CATEGORY_EXTERN(CategoryName)                                                \
    extern Helianthus::Common::LogCategory CategoryName

#define H_DEFINE_LOG_CATEGORY(CategoryName, DefaultVerbosity)                                      \
    Helianthus::Common::LogCategory& CategoryName =                                                \
        Helianthus::Common::LogCategory::GetInstance(#CategoryName, DefaultVerbosity)

// H_LOG(Category, Level, Fmt, ...): 仅输出到分类logger（控制台/分类文件）
#if HELIANTHUS_ENABLE_LOGGING
    #define H_LOG(Category, Level, Fmt, ...)                                                       \
        do                                                                                         \
        {                                                                                          \
            if (static_cast<int>(Level) <= static_cast<int>((Category).GetMinVerbosity()))         \
            {                                                                                      \
                Helianthus::Common::Logger::CategoryLog(                                           \
                    #Category,                                                                     \
                    Helianthus::Common::ToSpdLevel(Level),                                         \
                    spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                       \
                    Fmt,                                                                           \
                    ##__VA_ARGS__);                                                                \
            }                                                                                      \
        } while (0)
#else
    #define H_LOG(Category, Level, Fmt, ...)                                                       \
        do                                                                                         \
        {                                                                                          \
        } while (0)
#endif
