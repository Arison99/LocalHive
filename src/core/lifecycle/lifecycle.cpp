#include "localhive/core/lifecycle/lifecycle.hpp"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <csignal>
#endif

namespace localhive::core::lifecycle {

void CancellationToken::cancel() noexcept {
    if (cancelled_.exchange(true, std::memory_order_acq_rel)) {
        return; // Already cancelled
    }
    std::vector<std::function<void()>> cbs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cbs = callbacks_;
    }
    for (const auto& cb : cbs) {
        if (cb) {
            cb();
        }
    }
}

void CancellationToken::register_callback(std::function<void()> cb) {
    if (!cb) return;
    bool already_cancelled = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cancelled_.load(std::memory_order_acquire)) {
            already_cancelled = true;
        } else {
            callbacks_.push_back(std::move(cb));
        }
    }
    if (already_cancelled) {
        cb();
    }
}

ComponentRegistry::~ComponentRegistry() {
    shutdown_all(std::chrono::milliseconds(5000));
}

void ComponentRegistry::register_component(std::string name, std::shared_ptr<IService> component) {
    if (!component) return;
    std::lock_guard<std::mutex> lock(mutex_);
    components_.push_back({std::move(name), std::move(component)});
}

Status ComponentRegistry::shutdown_all(std::chrono::milliseconds per_component_timeout) {
    std::vector<Entry> to_stop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        to_stop = components_;
        components_.clear();
    }

    // Stop in REVERSE order
    Status overall = Status::Ok();
    for (auto it = to_stop.rbegin(); it != to_stop.rend(); ++it) {
        if (it->component) {
            it->component->request_stop();
            auto st = it->component->wait_for_stop(per_component_timeout);
            if (!st.ok() && overall.ok()) {
                overall = st;
            }
        }
    }
    return overall;
}

size_t ComponentRegistry::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return components_.size();
}

std::mutex SignalHandler::mutex_;
SignalHandler::Callback SignalHandler::callback_;
bool SignalHandler::installed_ = false;

#if defined(_WIN32) || defined(_WIN64)
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SignalHandler::trigger_manual_signal_for_testing();
            return TRUE;
        default:
            return FALSE;
    }
}
#else
static void posix_signal_handler(int /*sig*/) {
    SignalHandler::trigger_manual_signal_for_testing();
}
#endif

void SignalHandler::install(Callback on_shutdown_signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(on_shutdown_signal);
    if (!installed_) {
#if defined(_WIN32) || defined(_WIN64)
        ::SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
        struct sigaction sa{};
        sa.sa_handler = posix_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
#endif
        installed_ = true;
    }
}

void SignalHandler::uninstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
#if defined(_WIN32) || defined(_WIN64)
        ::SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
#else
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
#endif
        installed_ = false;
    }
    callback_ = nullptr;
}

void SignalHandler::trigger_manual_signal_for_testing() {
    Callback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = callback_;
    }
    if (cb) {
        cb();
    }
}

} // namespace localhive::core::lifecycle
