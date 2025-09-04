#include "Shared/Common/StructuredLogger.h"

#include "Shared/Common/Logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#if defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

// 防止 Windows 宏冲突
#ifdef _WIN32
    #ifdef DEBUG
        #undef DEBUG
    #endif
    #ifdef ERROR
        #undef ERROR
    #endif
    #ifdef INFO
        #undef INFO
    #endif
    #ifdef WARN
        #undef WARN
    #endif
#endif

namespace Helianthus::Common
{
// 静态成员初始化
std::unique_ptr<StructuredLogger> StructuredLogger::Instance = nullptr;
std::mutex StructuredLogger::InstanceMutex;
thread_local LogFields StructuredLogger::ThreadFields;

// JSON输出Sink（到ostream）
class JsonLogSink : public ILogSink
{
public:
    JsonLogSink(std::ostream& Output) : OutputStream(Output) {}

    void Write(const LogRecord& Record) override
    {
        std::lock_guard<std::mutex> Lock(WriteMutex);

        OutputStream << "{";
        OutputStream << "\"timestamp\":\"" << FormatTimestamp(Record.Timestamp) << "\",";
        OutputStream << "\"level\":\"" << LevelToString(Record.Level) << "\",";
        OutputStream << "\"category\":\"" << Record.Category << "\",";
        OutputStream << "\"message\":\"" << EscapeJsonString(Record.Message) << "\",";

        if (!Record.TraceId.empty())
            OutputStream << "\"trace_id\":\"" << Record.TraceId << "\",";
        if (!Record.SpanId.empty())
            OutputStream << "\"span_id\":\"" << Record.SpanId << "\",";
        if (!Record.ThreadId.empty())
            OutputStream << "\"thread_id\":\"" << Record.ThreadId << "\",";
        if (!Record.FileName.empty())
            OutputStream << "\"file\":\"" << Record.FileName << ":" << Record.LineNumber << "\",";
        if (!Record.FunctionName.empty())
            OutputStream << "\"function\":\"" << Record.FunctionName << "\",";

        // 输出字段
        if (!Record.Fields.GetAllFields().empty())
        {
            OutputStream << "\"fields\":{";
            bool First = true;
            for (const auto& [Key, Value] : Record.Fields.GetAllFields())
            {
                if (!First)
                    OutputStream << ",";
                OutputStream << "\"" << Key << "\":";
                OutputFieldValue(Value);
                First = false;
            }
            OutputStream << "}";
        }

        OutputStream << "}" << std::endl;
    }

    void Flush() override
    {
        OutputStream.flush();
    }

private:
    std::ostream& OutputStream;
    std::mutex WriteMutex;

    std::string FormatTimestamp(const std::chrono::system_clock::time_point& Timestamp)
    {
        auto TimeT = std::chrono::system_clock::to_time_t(Timestamp);
        auto Ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(Timestamp.time_since_epoch()) %
            1000;

        std::stringstream Ss;
        Ss << std::put_time(std::gmtime(&TimeT), "%Y-%m-%dT%H:%M:%S");
        Ss << "." << std::setfill('0') << std::setw(3) << Ms.count() << "Z";
        return Ss.str();
    }

    std::string LevelToString(StructuredLogLevel Level)
    {
        switch (Level)
        {
            case StructuredLogLevel::TRACE:
                return "TRACE";
            case StructuredLogLevel::DEBUG_LEVEL:
                return "DEBUG";
            case StructuredLogLevel::INFO:
                return "INFO";
            case StructuredLogLevel::WARN:
                return "WARN";
            case StructuredLogLevel::ERROR:
                return "ERROR";
            case StructuredLogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    std::string EscapeJsonString(const std::string& Str)
    {
        std::string Result;
        Result.reserve(Str.length());

        for (char C : Str)
        {
            switch (C)
            {
                case '"':
                    Result += "\\\"";
                    break;
                case '\\':
                    Result += "\\\\";
                    break;
                case '\b':
                    Result += "\\b";
                    break;
                case '\f':
                    Result += "\\f";
                    break;
                case '\n':
                    Result += "\\n";
                    break;
                case '\r':
                    Result += "\\r";
                    break;
                case '\t':
                    Result += "\\t";
                    break;
                default:
                    Result += C;
                    break;
            }
        }
        return Result;
    }

    void OutputFieldValue(const LogFieldValue& Value)
    {
        if (std::holds_alternative<std::string>(Value))
        {
            OutputStream << "\"" << EscapeJsonString(std::get<std::string>(Value)) << "\"";
        }
        else if (std::holds_alternative<bool>(Value))
        {
            OutputStream << (std::get<bool>(Value) ? "true" : "false");
        }
        else if (std::holds_alternative<int32_t>(Value))
        {
            OutputStream << std::get<int32_t>(Value);
        }
        else if (std::holds_alternative<int64_t>(Value))
        {
            OutputStream << std::get<int64_t>(Value);
        }
        else if (std::holds_alternative<uint32_t>(Value))
        {
            OutputStream << std::get<uint32_t>(Value);
        }
        else if (std::holds_alternative<uint64_t>(Value))
        {
            OutputStream << std::get<uint64_t>(Value);
        }
        else if (std::holds_alternative<double>(Value))
        {
            OutputStream << std::get<double>(Value);
        }
    }
};

// 基于spdlog旋转文件的JSON Sink
class RotatingFileJsonSink : public ILogSink
{
public:
    RotatingFileJsonSink(const std::string& FilePath, size_t MaxFileSize, size_t MaxFiles)
    {
        auto sink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(FilePath, MaxFileSize, MaxFiles);
        LoggerPtr = std::make_shared<spdlog::logger>("structured_json", sink);
        // 仅输出消息体本身
        LoggerPtr->set_pattern("%v");
        LoggerPtr->set_level(spdlog::level::info);
        LoggerPtr->flush_on(spdlog::level::info);
    }

    void Write(const LogRecord& Record) override
    {
        std::string json = BuildJson(Record);
        std::lock_guard<std::mutex> Lock(WriteMutex);
        LoggerPtr->info("{}", json);
    }

    void Flush() override
    {
        LoggerPtr->flush();
    }

private:
    std::shared_ptr<spdlog::logger> LoggerPtr;
    std::mutex WriteMutex;

    static std::string Escape(const std::string& Str)
    {
        std::string Result;
        Result.reserve(Str.length());
        for (char C : Str)
        {
            switch (C)
            {
                case '"':
                    Result += "\\\"";
                    break;
                case '\\':
                    Result += "\\\\";
                    break;
                case '\b':
                    Result += "\\b";
                    break;
                case '\f':
                    Result += "\\f";
                    break;
                case '\n':
                    Result += "\\n";
                    break;
                case '\r':
                    Result += "\\r";
                    break;
                case '\t':
                    Result += "\\t";
                    break;
                default:
                    Result += C;
                    break;
            }
        }
        return Result;
    }

    static std::string LevelToString(StructuredLogLevel Level)
    {
        switch (Level)
        {
            case StructuredLogLevel::TRACE:
                return "TRACE";
            case StructuredLogLevel::DEBUG_LEVEL:
                return "DEBUG";
            case StructuredLogLevel::INFO:
                return "INFO";
            case StructuredLogLevel::WARN:
                return "WARN";
            case StructuredLogLevel::ERROR:
                return "ERROR";
            case StructuredLogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    static std::string FormatTimestamp(const std::chrono::system_clock::time_point& Timestamp)
    {
        auto TimeT = std::chrono::system_clock::to_time_t(Timestamp);
        auto Ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(Timestamp.time_since_epoch()) %
            1000;

        std::stringstream Ss;
        Ss << std::put_time(std::gmtime(&TimeT), "%Y-%m-%dT%H:%M:%S");
        Ss << "." << std::setfill('0') << std::setw(3) << Ms.count() << "Z";
        return Ss.str();
    }

    static void AppendFieldValue(std::stringstream& Os, const LogFieldValue& Value)
    {
        if (std::holds_alternative<std::string>(Value))
        {
            Os << '"' << Escape(std::get<std::string>(Value)) << '"';
        }
        else if (std::holds_alternative<bool>(Value))
        {
            Os << (std::get<bool>(Value) ? "true" : "false");
        }
        else if (std::holds_alternative<int32_t>(Value))
        {
            Os << std::get<int32_t>(Value);
        }
        else if (std::holds_alternative<int64_t>(Value))
        {
            Os << std::get<int64_t>(Value);
        }
        else if (std::holds_alternative<uint32_t>(Value))
        {
            Os << std::get<uint32_t>(Value);
        }
        else if (std::holds_alternative<uint64_t>(Value))
        {
            Os << std::get<uint64_t>(Value);
        }
        else if (std::holds_alternative<double>(Value))
        {
            Os << std::get<double>(Value);
        }
    }

    // 轻量级进程名与PID工具（跨平台尽量覆盖）
    static std::string DetectProcessName()
    {
        // 尝试多平台获取可执行名，失败则返回空
#if defined(_WIN32)
        char Buffer[MAX_PATH] = {0};
        DWORD Len = GetModuleFileNameA(nullptr, Buffer, MAX_PATH);
        if (Len > 0)
        {
            std::string Path(Buffer, Buffer + Len);
            auto Pos = Path.find_last_of("\\/");
            return Pos == std::string::npos ? Path : Path.substr(Pos + 1);
        }
        return "";
#elif defined(__APPLE__)
        char Buffer[1024];
        uint32_t Size = sizeof(Buffer);
        if (_NSGetExecutablePath(Buffer, &Size) == 0)
        {
            std::string Path(Buffer);
            auto Pos = Path.find_last_of("/");
            return Pos == std::string::npos ? Path : Path.substr(Pos + 1);
        }
        return "";
#else
        // Linux/Unix: 先尝试 /proc/self/comm，再回退 /proc/self/exe
        try
        {
            std::ifstream Comm("/proc/self/comm");
            if (Comm.is_open())
            {
                std::string Name;
                std::getline(Comm, Name);
                if (!Name.empty())
                    return Name;
            }
        }
        catch (...)
        {
        }
        try
        {
            char Buffer[1024] = {0};
            ssize_t Len = readlink("/proc/self/exe", Buffer, sizeof(Buffer) - 1);
            if (Len > 0)
            {
                std::string Path(Buffer, Buffer + Len);
                auto Pos = Path.find_last_of("/");
                return Pos == std::string::npos ? Path : Path.substr(Pos + 1);
            }
        }
        catch (...)
        {
        }
        return "";
#endif
    }

    static uint64_t GetProcessId()
    {
#if defined(_WIN32)
        return static_cast<uint64_t>(GetCurrentProcessId());
#else
        return static_cast<uint64_t>(getpid());
#endif
    }

    static std::string BuildJson(const LogRecord& R)
    {
        std::stringstream Os;
        Os << '{';
        // 新标准键
        Os << "\"time\":\"" << FormatTimestamp(R.Timestamp) << "\",";
        Os << "\"level\":\"" << LevelToString(R.Level) << "\",";
        Os << "\"pid\":" << GetProcessId() << ",";
        if (!R.ThreadId.empty())
            Os << "\"tid\":\"" << R.ThreadId << "\",";
        {
            static const std::string ProcName = DetectProcessName();
            if (!ProcName.empty())
                Os << "\"proc_name\":\"" << ProcName << "\",";
        }
        Os << "\"category\":\"" << R.Category << "\",";
        if (!R.FileName.empty())
        {
            Os << "\"file_name\":\"" << R.FileName << "\",";
            Os << "\"line_no\":" << R.LineNumber << ",";
        }
        if (!R.TraceId.empty())
            Os << "\"cid\":\"" << R.TraceId << "\",";
        Os << "\"message\":\"" << Escape(R.Message) << "\",";

        // 兼容旧键（过渡期保留）
        Os << "\"timestamp\":\"" << FormatTimestamp(R.Timestamp) << "\",";
        if (!R.TraceId.empty())
            Os << "\"trace_id\":\"" << R.TraceId << "\",";
        if (!R.ThreadId.empty())
            Os << "\"thread_id\":\"" << R.ThreadId << "\",";
        if (!R.FileName.empty())
            Os << "\"file\":\"" << R.FileName << ":" << R.LineNumber << "\",";
        if (!R.FunctionName.empty())
            Os << "\"function\":\"" << R.FunctionName << "\",";
        if (!R.Fields.GetAllFields().empty())
        {
            Os << "\"fields\":{";
            bool first = true;
            for (const auto& [k, v] : R.Fields.GetAllFields())
            {
                if (!first)
                    Os << ',';
                Os << '"' << k << '"' << ':';
                AppendFieldValue(Os, v);
                first = false;
            }
            Os << "}";
        }
        Os << '}';
        return Os.str();
    }
};

// 通过现有Logger模块输出的Sink
class LoggerBasedSink : public ILogSink
{
public:
    void Write(const LogRecord& Record) override
    {
        // 构建格式化的日志消息
        std::stringstream Message;
        Message << "[" << Record.Category << "] " << Record.Message;

        if (!Record.TraceId.empty())
            Message << " [trace_id=" << Record.TraceId << "]";

        // 输出字段
        if (!Record.Fields.GetAllFields().empty())
        {
            Message << " {";
            bool First = true;
            for (const auto& [Key, Value] : Record.Fields.GetAllFields())
            {
                if (!First)
                {
                    Message << ", ";
                }
                Message << Key << "=";
                OutputFieldValue(Value, Message);
                First = false;
            }
            Message << "}";
        }

        // 使用项目包装的分类日志，携带准确的文件与行号
        spdlog::level::level_enum Level = spdlog::level::info;
        switch (Record.Level)
        {
            case StructuredLogLevel::TRACE:
                Level = spdlog::level::trace;
                break;
            case StructuredLogLevel::DEBUG_LEVEL:
                Level = spdlog::level::debug;
                break;
            case StructuredLogLevel::INFO:
                Level = spdlog::level::info;
                break;
            case StructuredLogLevel::WARN:
                Level = spdlog::level::warn;
                break;
            case StructuredLogLevel::ERROR:
                Level = spdlog::level::err;
                break;
            case StructuredLogLevel::FATAL:
                Level = spdlog::level::critical;
                break;
            default:
                break;
        }

        const char* file = Record.FileName.empty() ? __FILE__ : Record.FileName.c_str();
        const char* func =
            Record.FunctionName.empty() ? SPDLOG_FUNCTION : Record.FunctionName.c_str();
        Helianthus::Common::Logger::CategoryLog(
            Record.Category.c_str(),
            Level,
            spdlog::source_loc{file, static_cast<int>(Record.LineNumber), func},
            "{}",
            Message.str());
    }

    void Flush() override
    {
        Logger::Flush();
    }

private:
    void OutputFieldValue(const LogFieldValue& Value, std::stringstream& Stream)
    {
        if (std::holds_alternative<std::string>(Value))
        {
            Stream << "\"" << std::get<std::string>(Value) << "\"";
        }
        else if (std::holds_alternative<bool>(Value))
        {
            Stream << (std::get<bool>(Value) ? "true" : "false");
        }
        else if (std::holds_alternative<int32_t>(Value))
        {
            Stream << std::get<int32_t>(Value);
        }
        else if (std::holds_alternative<int64_t>(Value))
        {
            Stream << std::get<int64_t>(Value);
        }
        else if (std::holds_alternative<uint32_t>(Value))
        {
            Stream << std::get<uint32_t>(Value);
        }
        else if (std::holds_alternative<uint64_t>(Value))
        {
            Stream << std::get<uint64_t>(Value);
        }
        else if (std::holds_alternative<double>(Value))
        {
            Stream << std::get<double>(Value);
        }
    }
};

// StructuredLogger构造函数
StructuredLogger::StructuredLogger(const StructuredLoggerConfig& Config)
    : Config(Config), IsShutdown(false)
{
}

// StructuredLogger实现
void StructuredLogger::Initialize(const StructuredLoggerConfig& Config)
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);

    if (Instance)
    {
        return;  // 已经初始化
    }

    Instance = std::unique_ptr<StructuredLogger>(new StructuredLogger(Config));

    // 根据配置添加输出端
    if (Config.EnableConsole)
    {
        Instance->AddSink(std::make_shared<LoggerBasedSink>());
    }

    if (Config.EnableFile)
    {
        try
        {
            std::filesystem::path p = Config.FilePath;
            if (p.has_parent_path())
            {
                std::filesystem::create_directories(p.parent_path());
            }
        }
        catch (...)
        {
            // 目录创建失败时忽略，后续sink创建可能会报错
        }
        Instance->AddSink(std::make_shared<RotatingFileJsonSink>(
            Config.FilePath, Config.MaxFileSize, Config.MaxFiles));
    }
}

void StructuredLogger::Shutdown()
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);

    if (Instance)
    {
        Instance->IsShutdown = true;

        // 刷新所有输出端
        std::lock_guard<std::mutex> SinkLock(Instance->SinksMutex);
        for (auto& Sink : Instance->Sinks)
        {
            Sink->Flush();
        }

        Instance.reset();
    }
}

bool StructuredLogger::IsInitialized()
{
    std::lock_guard<std::mutex> Lock(InstanceMutex);
    return Instance != nullptr;
}

void StructuredLogger::Log(StructuredLogLevel Level,
                           const std::string& Category,
                           const std::string& Message,
                           const LogFields& Fields,
                           const std::string& FileName,
                           uint32_t LineNumber,
                           const std::string& FunctionName)
{
    if (!IsInitialized() || Level < Instance->Config.MinLevel)
    {
        return;
    }

    auto Record = Instance->CreateLogRecord(
        Level, Category, Message, Fields, FileName, LineNumber, FunctionName);
    Instance->WriteToSinks(Record);
}

void StructuredLogger::Trace(const std::string& Category,
                             const std::string& Message,
                             const LogFields& Fields)
{
    Log(StructuredLogLevel::TRACE, Category, Message, Fields);
}

void StructuredLogger::Debug(const std::string& Category,
                             const std::string& Message,
                             const LogFields& Fields)
{
    Log(StructuredLogLevel::DEBUG_LEVEL, Category, Message, Fields);
}

void StructuredLogger::Info(const std::string& Category,
                            const std::string& Message,
                            const LogFields& Fields)
{
    Log(StructuredLogLevel::INFO, Category, Message, Fields);
}

void StructuredLogger::Warn(const std::string& Category,
                            const std::string& Message,
                            const LogFields& Fields)
{
    Log(StructuredLogLevel::WARN, Category, Message, Fields);
}

void StructuredLogger::Error(const std::string& Category,
                             const std::string& Message,
                             const LogFields& Fields)
{
    Log(StructuredLogLevel::ERROR, Category, Message, Fields);
}

void StructuredLogger::Fatal(const std::string& Category,
                             const std::string& Message,
                             const LogFields& Fields)
{
    Log(StructuredLogLevel::FATAL, Category, Message, Fields);
}

void StructuredLogger::SetGlobalField(const std::string& Key, const std::string& Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, int32_t Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, int64_t Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, uint32_t Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, uint64_t Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, double Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

void StructuredLogger::SetGlobalField(const std::string& Key, bool Value)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.AddField(Key, Value);
    }
}

// 线程本地字段实现
void StructuredLogger::SetThreadField(const std::string& Key, const std::string& Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, int32_t Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, int64_t Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, uint32_t Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, uint64_t Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, double Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::SetThreadField(const std::string& Key, bool Value)
{
    ThreadFields.AddField(Key, Value);
}
void StructuredLogger::ClearThreadField(const std::string& Key)
{
    (void)Key;  // LogFields暂无单独删除，实现前先保留接口
}
void StructuredLogger::ClearAllThreadFields()
{
    ThreadFields.Clear();
}

void StructuredLogger::ClearGlobalField(const std::string& Key)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        // 注意：LogFields目前没有RemoveField方法，这里暂时跳过
        (void)Key;  // 防止未使用参数警告
    }
}

void StructuredLogger::ClearAllGlobalFields()
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->GlobalFieldsMutex);
        Instance->GlobalFields.Clear();
    }
}

void StructuredLogger::AddSink(std::shared_ptr<ILogSink> Sink)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->SinksMutex);
        Instance->Sinks.push_back(Sink);
    }
}

void StructuredLogger::RemoveSink(std::shared_ptr<ILogSink> Sink)
{
    if (Instance)
    {
        std::lock_guard<std::mutex> Lock(Instance->SinksMutex);
        Instance->Sinks.erase(std::remove(Instance->Sinks.begin(), Instance->Sinks.end(), Sink),
                              Instance->Sinks.end());
    }
}

void StructuredLogger::RecordMetric(const std::string& Name, double Value, const LogFields& Tags)
{
    LogFields MetricFields = Tags;
    MetricFields.AddField("metric_name", Name);
    MetricFields.AddField("metric_value", Value);
    MetricFields.AddField("metric_type", "gauge");

    Log(StructuredLogLevel::INFO, "METRICS", "Metric recorded", MetricFields);
}

void StructuredLogger::IncrementCounter(const std::string& Name,
                                        int64_t Value,
                                        const LogFields& Tags)
{
    LogFields CounterFields = Tags;
    CounterFields.AddField("counter_name", Name);
    CounterFields.AddField("counter_value", Value);
    CounterFields.AddField("metric_type", "counter");

    Log(StructuredLogLevel::INFO, "METRICS", "Counter incremented", CounterFields);
}

void StructuredLogger::RecordHistogram(const std::string& Name, double Value, const LogFields& Tags)
{
    LogFields HistogramFields = Tags;
    HistogramFields.AddField("histogram_name", Name);
    HistogramFields.AddField("histogram_value", Value);
    HistogramFields.AddField("metric_type", "histogram");

    Log(StructuredLogLevel::INFO, "METRICS", "Histogram recorded", HistogramFields);
}

// 私有方法实现
void StructuredLogger::WriteToSinks(const LogRecord& Record)
{
    std::lock_guard<std::mutex> Lock(SinksMutex);
    for (auto& Sink : Sinks)
    {
        try
        {
            Sink->Write(Record);
        }
        catch (...)
        {
            // 忽略输出端错误，避免影响主程序
        }
    }
}

LogRecord StructuredLogger::CreateLogRecord(StructuredLogLevel Level,
                                            const std::string& Category,
                                            const std::string& Message,
                                            const LogFields& Fields,
                                            const std::string& FileName,
                                            uint32_t LineNumber,
                                            const std::string& FunctionName)
{
    LogRecord Record;
    Record.Timestamp = std::chrono::system_clock::now();
    Record.Level = Level;
    Record.Category = Category;
    Record.Message = Message;
    Record.FileName = FileName;
    Record.LineNumber = LineNumber;
    Record.FunctionName = FunctionName;
    Record.ThreadId = GetCurrentThreadId();
    Record.TraceId = GenerateTraceId();

    // 合并全局字段和记录字段
    Record.Fields = Fields;
    {
        std::lock_guard<std::mutex> Lock(GlobalFieldsMutex);
        Record.Fields.Merge(GlobalFields);
    }
    // 合并线程本地字段（最后覆盖）
    Record.Fields.Merge(ThreadFields);

    return Record;
}

std::string StructuredLogger::GenerateTraceId()
{
    static std::random_device Rd;
    static std::mt19937 Gen(Rd());
    static std::uniform_int_distribution<uint64_t> Dis(0, UINT64_MAX);

    std::stringstream Ss;
    Ss << std::hex << std::setfill('0') << std::setw(16) << Dis(Gen);
    return Ss.str();
}

std::string StructuredLogger::GetCurrentThreadId()
{
    std::stringstream Ss;
    Ss << std::this_thread::get_id();
    return Ss.str();
}

// LogScope实现
LogScope::LogScope(const std::string& Category,
                   const std::string& Operation,
                   const LogFields& Fields)
    : Category(Category),
      Operation(Operation),
      Fields(Fields),
      StartTime(std::chrono::steady_clock::now())
{
    LogFields StartFields = Fields;
    StartFields.AddField("operation", Operation);
    StartFields.AddField("event", "start");

    StructuredLogger::Info(Category, "Operation started", StartFields);
}

LogScope::~LogScope()
{
    if (!IsCompleted)
    {
        Complete();
    }
}

void LogScope::AddField(const std::string& Key, const std::string& Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, int32_t Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, int64_t Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, uint32_t Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, uint64_t Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, double Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::AddField(const std::string& Key, bool Value)
{
    Fields.AddField(Key, Value);
}

void LogScope::LogEvent(const std::string& Event, const LogFields& AdditionalFields)
{
    LogFields EventFields = Fields;
    EventFields.Merge(AdditionalFields);
    EventFields.AddField("operation", Operation);
    EventFields.AddField("event", Event);

    StructuredLogger::Info(Category, "Operation event", EventFields);
}

void LogScope::Complete()
{
    if (IsCompleted)
        return;

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    LogFields EndFields = Fields;
    EndFields.AddField("operation", Operation);
    EndFields.AddField("event", "complete");
    EndFields.AddField("duration_ms", static_cast<int64_t>(Duration.count()));

    StructuredLogger::Info(Category, "Operation completed", EndFields);

    IsCompleted = true;
}

// PerformanceScope实现
PerformanceScope::PerformanceScope(const std::string& Operation, const LogFields& Tags)
    : Operation(Operation), Tags(Tags), StartTime(std::chrono::steady_clock::now())
{
    LogFields StartFields = Tags;
    StartFields.AddField("operation", Operation);
    StartFields.AddField("event", "perf_start");

    StructuredLogger::Debug("PERFORMANCE", "Performance monitoring started", StartFields);
}

PerformanceScope::~PerformanceScope()
{
    if (!IsCompleted)
    {
        Complete();
    }
}

void PerformanceScope::AddTag(const std::string& Key, const std::string& Value)
{
    Tags.AddField(Key, Value);
}

void PerformanceScope::AddTag(const std::string& Key, int32_t Value)
{
    Tags.AddField(Key, Value);
}

void PerformanceScope::AddTag(const std::string& Key, int64_t Value)
{
    Tags.AddField(Key, Value);
}

void PerformanceScope::Complete()
{
    if (IsCompleted)
        return;

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    LogFields EndFields = Tags;
    EndFields.AddField("operation", Operation);
    EndFields.AddField("event", "perf_complete");
    EndFields.AddField("duration_us", static_cast<int64_t>(Duration.count()));

    StructuredLogger::RecordHistogram(Operation + "_duration", Duration.count() / 1000.0, Tags);
    StructuredLogger::Debug("PERFORMANCE", "Performance monitoring completed", EndFields);

    IsCompleted = true;
}

}  // namespace Helianthus::Common
