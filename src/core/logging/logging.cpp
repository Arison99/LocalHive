#include "localhive/core/logging/logging.hpp"
#include "localhive/core/platform/platform.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace localhive::core::logging {

std::string_view log_level_to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

LogLevel parse_log_level(std::string_view str) noexcept {
    if (str == "TRACE" || str == "trace") return LogLevel::Trace;
    if (str == "DEBUG" || str == "debug") return LogLevel::Debug;
    if (str == "INFO"  || str == "info")  return LogLevel::Info;
    if (str == "WARN"  || str == "warn")  return LogLevel::Warn;
    if (str == "ERROR" || str == "error") return LogLevel::Error;
    if (str == "FATAL" || str == "fatal") return LogLevel::Fatal;
    return LogLevel::Info;
}

ConsoleSink::ConsoleSink(bool use_stderr_for_errors)
    : use_stderr_(use_stderr_for_errors) {}

void ConsoleSink::write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    std::ostream& out = (use_stderr_ && (record.level == LogLevel::Error || record.level == LogLevel::Fatal))
                            ? std::cerr
                            : std::cout;

    // Formatting: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [file:line] message
    out << "[" << record.timestamp_ms << " ms] "
        << "[" << log_level_to_string(record.level) << "] "
        << "[" << record.file << ":" << record.line << "] "
        << record.message << "\n";
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    std::cout.flush();
    std::cerr.flush();
}

Logger::Logger() {
    sinks_.push_back(std::make_shared<ConsoleSink>());
}

Logger::~Logger() {
    flush();
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

bool Logger::should_log(LogLevel level) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(level_);
}

void Logger::add_sink(std::shared_ptr<ILogSink> sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, std::string_view file, uint32_t line, std::string_view func, std::string message) {
    if (!should_log(level)) return;

    LogRecord record;
    record.level = level;
    record.timestamp_ms = platform::monotonic_now_ms();
    record.file = file;
    record.line = line;
    record.function = func;
    record.message = redact(message);

    std::vector<std::shared_ptr<ILogSink>> current_sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_sinks = sinks_;
    }

    for (const auto& sink : current_sinks) {
        sink->write(record);
    }
}

void Logger::flush() {
    std::vector<std::shared_ptr<ILogSink>> current_sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_sinks = sinks_;
    }
    for (const auto& sink : current_sinks) {
        sink->flush();
    }
}

std::string Logger::redact(std::string_view input) {
    std::string text(input);
    const std::vector<std::string> patterns = {
        "Authorization: Bearer ",
        "AWS4-HMAC-SHA256 Credential=",
        "password=",
        "secret="
    };

    for (const auto& pat : patterns) {
        size_t pos = 0;
        while ((pos = text.find(pat, pos)) != std::string::npos) {
            size_t val_start = pos + pat.size();
            size_t val_end = text.find_first_of(" \r\n;&", val_start);
            if (val_end == std::string::npos) {
                val_end = text.size();
            }
            if (val_end > val_start) {
                text.replace(val_start, val_end - val_start, "[REDACTED]");
                pos = val_start + 10;
            } else {
                pos = val_start;
            }
        }
    }
    return text;
}

} // namespace localhive::core::logging
