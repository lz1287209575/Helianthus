#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <variant>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include <fstream>
#include <vector>

namespace Helianthus::Common
{
    // 日志字段值类型
    using LogFieldValue = std::variant<
        std::string,
        int32_t,
        int64_t,
        uint32_t,
        uint64_t,
        double,
        bool
    >;

    // 日志级别
    enum class StructuredLogLevel
    {
        TRACE = 0,
        DEBUG_LEVEL,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    // 日志字段集合
    class LogFields
    {
    public:
        LogFields() = default;
        LogFields(const LogFields&) = default;
        LogFields(LogFields&&) = default;
        LogFields& operator=(const LogFields&) = default;
        LogFields& operator=(LogFields&&) = default;

        // 添加字段
        void AddField(const std::string& Key, const std::string& Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, int32_t Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, int64_t Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, uint32_t Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, uint64_t Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, double Value) { Fields[Key] = Value; }
        void AddField(const std::string& Key, bool Value) { Fields[Key] = Value; }

        // 获取字段
        const LogFieldValue* GetField(const std::string& Key) const
        {
            auto It = Fields.find(Key);
            return It != Fields.end() ? &It->second : nullptr;
        }

        // 合并字段
        void Merge(const LogFields& Other)
        {
            Fields.insert(Other.Fields.begin(), Other.Fields.end());
        }

        // 清空字段
        void Clear() { Fields.clear(); }

        // 获取所有字段
        const std::unordered_map<std::string, LogFieldValue>& GetAllFields() const { return Fields; }

    private:
        std::unordered_map<std::string, LogFieldValue> Fields;
    };

    // 日志记录
    struct LogRecord
    {
        std::chrono::system_clock::time_point Timestamp;
        StructuredLogLevel Level;
        std::string Category;
        std::string Message;
        LogFields Fields;
        std::string TraceId;
        std::string SpanId;
        std::string ThreadId;
        std::string FileName;
        uint32_t LineNumber;
        std::string FunctionName;
    };

    // 日志输出接口
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;
        virtual void Write(const LogRecord& Record) = 0;
        virtual void Flush() = 0;
    };

    // 结构日志器配置
    struct StructuredLoggerConfig
    {
        StructuredLogLevel MinLevel = StructuredLogLevel::INFO;
        bool EnableConsole = true;
        bool EnableFile = true;
        std::string FilePath = "logs/structured.log";
        size_t MaxFileSize = 100 * 1024 * 1024; // 100MB
        size_t MaxFiles = 10;
        bool EnableJsonOutput = true;
        bool EnablePerformanceMetrics = true;
        size_t BufferSize = 8192;
        bool UseAsync = true;
        size_t WorkerThreads = 2;
    };

    // 结构日志器
    class StructuredLogger
    {
    public:
        explicit StructuredLogger(const StructuredLoggerConfig& Config);
        ~StructuredLogger() = default;
        
        static void Initialize(const StructuredLoggerConfig& Config = StructuredLoggerConfig());
        static void Shutdown();
        static bool IsInitialized();

        // 设置全局字段（会添加到所有日志记录中）
        static void SetGlobalField(const std::string& Key, const std::string& Value);
        static void SetGlobalField(const std::string& Key, int32_t Value);
        static void SetGlobalField(const std::string& Key, int64_t Value);
        static void SetGlobalField(const std::string& Key, uint32_t Value);
        static void SetGlobalField(const std::string& Key, uint64_t Value);
        static void SetGlobalField(const std::string& Key, double Value);
        static void SetGlobalField(const std::string& Key, bool Value);

        // 清除全局字段
        static void ClearGlobalField(const std::string& Key);
        static void ClearAllGlobalFields();

        // 日志记录方法
        static void Log(StructuredLogLevel Level, 
                       const std::string& Category,
                       const std::string& Message,
                       const LogFields& Fields = LogFields(),
                       const std::string& FileName = "",
                       uint32_t LineNumber = 0,
                       const std::string& FunctionName = "");

        // 便捷方法
        static void Trace(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());
        static void Debug(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());
        static void Info(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());
        static void Warn(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());
        static void Error(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());
        static void Fatal(const std::string& Category, const std::string& Message, const LogFields& Fields = LogFields());

        // 添加自定义输出端
        static void AddSink(std::shared_ptr<ILogSink> Sink);
        static void RemoveSink(std::shared_ptr<ILogSink> Sink);

        // 性能指标
        static void RecordMetric(const std::string& Name, double Value, const LogFields& Tags = LogFields());
        static void IncrementCounter(const std::string& Name, int64_t Value = 1, const LogFields& Tags = LogFields());
        static void RecordHistogram(const std::string& Name, double Value, const LogFields& Tags = LogFields());

    private:
        static std::unique_ptr<StructuredLogger> Instance;
        static std::mutex InstanceMutex;

        StructuredLoggerConfig Config;
        std::vector<std::shared_ptr<ILogSink>> Sinks;
        LogFields GlobalFields;
        std::mutex SinksMutex;
        std::mutex GlobalFieldsMutex;
        std::atomic<bool> IsShutdown{false};

        // 内部方法
        void WriteToSinks(const LogRecord& Record);
        LogRecord CreateLogRecord(StructuredLogLevel Level,
                                 const std::string& Category,
                                 const std::string& Message,
                                 const LogFields& Fields,
                                 const std::string& FileName,
                                 uint32_t LineNumber,
                                 const std::string& FunctionName);
        std::string GenerateTraceId();
        std::string GetCurrentThreadId();
    };

    // 日志作用域（自动添加字段）
    class LogScope
    {
    public:
        LogScope(const std::string& Category, const std::string& Operation, const LogFields& Fields = LogFields());
        ~LogScope();

        // 添加字段到当前作用域
        void AddField(const std::string& Key, const std::string& Value);
        void AddField(const std::string& Key, int32_t Value);
        void AddField(const std::string& Key, int64_t Value);
        void AddField(const std::string& Key, uint32_t Value);
        void AddField(const std::string& Key, uint64_t Value);
        void AddField(const std::string& Key, double Value);
        void AddField(const std::string& Key, bool Value);

        // 记录事件
        void LogEvent(const std::string& Event, const LogFields& AdditionalFields = LogFields());
        
        // 完成操作
        void Complete();

    private:
        std::string Category;
        std::string Operation;
        LogFields Fields;
        std::chrono::steady_clock::time_point StartTime;
        bool IsCompleted = false;
    };

    // 性能监控作用域
    class PerformanceScope
    {
    public:
        PerformanceScope(const std::string& Operation, const LogFields& Tags = LogFields());
        ~PerformanceScope();

        // 添加标签
        void AddTag(const std::string& Key, const std::string& Value);
        void AddTag(const std::string& Key, int32_t Value);
        void AddTag(const std::string& Key, int64_t Value);
        
        // 完成操作
        void Complete();

    private:
        std::string Operation;
        LogFields Tags;
        std::chrono::steady_clock::time_point StartTime;
        bool IsCompleted = false;
    };

} // namespace Helianthus::Common

// 便捷宏
#define HELIANTHUS_STRUCTURED_LOG_TRACE(category, message, ...) \
    Helianthus::Common::StructuredLogger::Trace(category, message, __VA_ARGS__)

#define HELIANTHUS_STRUCTURED_LOG_DEBUG(category, message, ...) \
    Helianthus::Common::StructuredLogger::Debug(category, message, __VA_ARGS__)

#define HELIANTHUS_STRUCTURED_LOG_INFO(category, message, ...) \
    Helianthus::Common::StructuredLogger::Info(category, message, __VA_ARGS__)

#define HELIANTHUS_STRUCTURED_LOG_WARN(category, message, ...) \
    Helianthus::Common::StructuredLogger::Warn(category, message, __VA_ARGS__)

#define HELIANTHUS_STRUCTURED_LOG_ERROR(category, message, ...) \
    Helianthus::Common::StructuredLogger::Error(category, message, __VA_ARGS__)

#define HELIANTHUS_STRUCTURED_LOG_FATAL(category, message, ...) \
    Helianthus::Common::StructuredLogger::Fatal(category, message, __VA_ARGS__)

// 日志作用域宏
#define HELIANTHUS_LOG_SCOPE(category, operation, ...) \
    Helianthus::Common::LogScope logScope(category, operation, __VA_ARGS__)

// 性能监控宏
#define HELIANTHUS_PERFORMANCE_SCOPE(operation, ...) \
    Helianthus::Common::PerformanceScope perfScope(operation, __VA_ARGS__)
