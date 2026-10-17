#ifndef LOCALHIVE_CORE_LOGGING_HPP
#define LOCALHIVE_CORE_LOGGING_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace localhive::core::logging {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

[[nodiscard]] std::string_view log_level_to_string(LogLevel level) noexcept;
[[nodiscard]] LogLevel parse_log_level(std::string_view str) noexcept;

struct LogRecord {
    LogLevel level{LogLevel::Info};
    uint64_t timestamp_ms{0};
    std::string_view file;
    uint32_t line{0};
    std::string_view function;
    std::string message;
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public ILogSink {
public:
    explicit ConsoleSink(bool use_stderr_for_errors = true);
    void write(const LogRecord& record) override;
    void flush() override;

private:
    bool use_stderr_;
    std::mutex sink_mutex_;
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;
    [[nodiscard]] bool should_log(LogLevel level) const noexcept;

    void add_sink(std::shared_ptr<ILogSink> sink);
    void clear_sinks();

    void log(LogLevel level, std::string_view file, uint32_t line, std::string_view func, std::string message);
    void flush();

    // Redaction utility: masks authorization headers and secret tokens
    static std::string redact(std::string_view input);

private:
    Logger();
    ~Logger();

    LogLevel level_{LogLevel::Info};
    std::vector<std::shared_ptr<ILogSink>> sinks_;
    mutable std::mutex mutex_;
};

#define LH_LOG(level, msg) \
    do { \
        if (::localhive::core::logging::Logger::instance().should_log(level)) { \
            ::localhive::core::logging::Logger::instance().log(level, __FILE__, __LINE__, __func__, (msg)); \
        } \
    } while (0)

#define LH_LOG_TRACE(msg) LH_LOG(::localhive::core::logging::LogLevel::Trace, msg)
#define LH_LOG_DEBUG(msg) LH_LOG(::localhive::core::logging::LogLevel::Debug, msg)
#define LH_LOG_INFO(msg)  LH_LOG(::localhive::core::logging::LogLevel::Info,  msg)
#define LH_LOG_WARN(msg)  LH_LOG(::localhive::core::logging::LogLevel::Warn,  msg)
#define LH_LOG_ERROR(msg) LH_LOG(::localhive::core::logging::LogLevel::Error, msg)
#define LH_LOG_FATAL(msg) LH_LOG(::localhive::core::logging::LogLevel::Fatal, msg)

} // namespace localhive::core::logging

#endif // LOCALHIVE_CORE_LOGGING_HPP
