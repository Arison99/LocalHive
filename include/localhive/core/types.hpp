#ifndef LOCALHIVE_CORE_TYPES_HPP
#define LOCALHIVE_CORE_TYPES_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <utility>

namespace localhive::core {

enum class StatusCode : uint16_t {
    Ok = 0,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    PermissionDenied,
    ResourceExhausted,
    FailedPrecondition,
    Aborted,
    OutOfRange,
    Unimplemented,
    Internal,
    Unavailable,
    DataLoss,
    Unauthenticated,
    Timeout,
    ConnectionReset
};

constexpr std::string_view status_code_to_string(StatusCode code) noexcept {
    switch (code) {
        case StatusCode::Ok: return "Ok";
        case StatusCode::InvalidArgument: return "InvalidArgument";
        case StatusCode::NotFound: return "NotFound";
        case StatusCode::AlreadyExists: return "AlreadyExists";
        case StatusCode::PermissionDenied: return "PermissionDenied";
        case StatusCode::ResourceExhausted: return "ResourceExhausted";
        case StatusCode::FailedPrecondition: return "FailedPrecondition";
        case StatusCode::Aborted: return "Aborted";
        case StatusCode::OutOfRange: return "OutOfRange";
        case StatusCode::Unimplemented: return "Unimplemented";
        case StatusCode::Internal: return "Internal";
        case StatusCode::Unavailable: return "Unavailable";
        case StatusCode::DataLoss: return "DataLoss";
        case StatusCode::Unauthenticated: return "Unauthenticated";
        case StatusCode::Timeout: return "Timeout";
        case StatusCode::ConnectionReset: return "ConnectionReset";
        default: return "Unknown";
    }
}

class Status {
public:
    Status() noexcept : code_(StatusCode::Ok) {}
    
    Status(StatusCode code, std::string message, std::string detail = "")
        : code_(code), message_(std::move(message)), detail_(std::move(detail)) {}

    [[nodiscard]] constexpr bool ok() const noexcept { return code_ == StatusCode::Ok; }
    [[nodiscard]] constexpr StatusCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

    [[nodiscard]] std::string to_string() const {
        if (ok()) return "OK";
        std::string res = std::string(status_code_to_string(code_));
        if (!message_.empty()) {
            res += ": ";
            res += message_;
        }
        if (!detail_.empty()) {
            res += " (";
            res += detail_;
            res += ")";
        }
        return res;
    }

    [[nodiscard]] static Status Ok() noexcept { return Status(); }
    [[nodiscard]] static Status Error(StatusCode c, std::string msg, std::string d = "") {
        return Status(c, std::move(msg), std::move(d));
    }

private:
    StatusCode code_{StatusCode::Ok};
    std::string message_;
    std::string detail_;
};

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), status_(Status::Ok()) {}
    Result(Status status) : status_(std::move(status)) {}
    Result(StatusCode code, std::string msg) : status_(Status::Error(code, std::move(msg))) {}

    [[nodiscard]] bool ok() const noexcept { return status_.ok(); }
    [[nodiscard]] const Status& status() const noexcept { return status_; }
    
    [[nodiscard]] T& value() & { return *value_; }
    [[nodiscard]] const T& value() const & { return *value_; }
    [[nodiscard]] T&& value() && { return std::move(*value_); }

    [[nodiscard]] T value_or(T default_val) const {
        return ok() ? *value_ : std::move(default_val);
    }

private:
    std::optional<T> value_;
    Status status_;
};

} // namespace localhive::core

#endif // LOCALHIVE_CORE_TYPES_HPP
