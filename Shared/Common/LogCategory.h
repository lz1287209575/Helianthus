#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "Common/Types.h"
#include "Common/Logger.h"

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
        explicit LogCategory(const char* NameIn, LogVerbosity DefaultVerbosity = LogVerbosity::Log)
            : Name(NameIn), MinVerbosity(DefaultVerbosity) {}

        const char* GetName() const { return Name; }

        void SetMinVerbosity(LogVerbosity Verbosity) { MinVerbosity.store(Verbosity, std::memory_order_relaxed); }
        LogVerbosity GetMinVerbosity() const { return MinVerbosity.load(std::memory_order_relaxed); }

        static void SetCategoryMinVerbosity(const std::string& CategoryName, LogVerbosity Verbosity)
        {
            std::lock_guard<std::mutex> Lock(GetRegistryMutex());
            auto& Map = GetRegistry();
            if (auto It = Map.find(CategoryName); It != Map.end())
            {
                It->second->SetMinVerbosity(Verbosity);
            }
        }

        static LogCategory* Register(const char* CategoryName, LogCategory* Category)
        {
            std::lock_guard<std::mutex> Lock(GetRegistryMutex());
            GetRegistry().emplace(CategoryName, Category);
            return Category;
        }

    private:
        static std::unordered_map<std::string, LogCategory*>& GetRegistry()
        {
            static std::unordered_map<std::string, LogCategory*> CategoryRegistry;
            return CategoryRegistry;
        }
        static std::mutex& GetRegistryMutex()
        {
            static std::mutex RegistryLock;
            return RegistryLock;
        }

        const char* Name;
        std::atomic<LogVerbosity> MinVerbosity;
    };

    inline spdlog::level::level_enum ToSpdLevel(LogVerbosity Verbosity)
    {
        switch (Verbosity)
        {
        case LogVerbosity::Fatal: return spdlog::level::critical;
        case LogVerbosity::Error: return spdlog::level::err;
        case LogVerbosity::Warning: return spdlog::level::warn;
        case LogVerbosity::Display: return spdlog::level::info;
        case LogVerbosity::Log: return spdlog::level::info;
        case LogVerbosity::Verbose: return spdlog::level::debug;
        case LogVerbosity::VeryVerbose: return spdlog::level::trace;
        }
        return spdlog::level::info;
    }
}

// 宏接口
#define H_DECLARE_LOG_CATEGORY_EXTERN(CategoryName) \
    extern Helianthus::Common::LogCategory CategoryName

#define H_DEFINE_LOG_CATEGORY(CategoryName, DefaultVerbosity) \
    Helianthus::Common::LogCategory CategoryName##Impl{#CategoryName, DefaultVerbosity}; \
    Helianthus::Common::LogCategory& CategoryName = *Helianthus::Common::LogCategory::Register(#CategoryName, &CategoryName##Impl)

// H_LOG(Category, Level, Fmt, ...): 输出到主logger与已配置的分类文件logger
#define H_LOG(Category, Level, Fmt, ...)                                                                                  \
    do {                                                                                                                  \
        if (static_cast<int>(Level) <= static_cast<int>((Category).GetMinVerbosity())) {                                  \
            /* 主Logger（含控制台与全局滚动文件） */                                                                      \
            if (Level == Helianthus::Common::LogVerbosity::Warning) {                                                     \
                Helianthus::Common::Logger::Warn("[{}] " Fmt, (Category).GetName(), ##__VA_ARGS__);                     \
            } else if (Level == Helianthus::Common::LogVerbosity::Error || Level == Helianthus::Common::LogVerbosity::Fatal) { \
                Helianthus::Common::Logger::Error("[{}] " Fmt, (Category).GetName(), ##__VA_ARGS__);                    \
            } else {                                                                                                      \
                Helianthus::Common::Logger::Info("[{}] " Fmt, (Category).GetName(), ##__VA_ARGS__);                      \
            }                                                                                                             \
            /* 分类专用文件logger（如已配置） */                                                                           \
            Helianthus::Common::Logger::CategoryLog((Category).GetName(), Helianthus::Common::ToSpdLevel(Level),          \
                                                    "[{}] " Fmt, (Category).GetName(), ##__VA_ARGS__);                   \
        }                                                                                                                 \
    } while (0)
