#include "watchdog.hpp"
#include "logger.hpp"

namespace industrial {

Watchdog::Watchdog(uint32_t timeout_ms)
    : timeout_ms_(timeout_ms),
      last_kick_time_ms_(get_steady_time_ms()),
      healthy_(true),
      tripped_(false) {}

void Watchdog::kick() {
    last_kick_time_ms_.store(get_steady_time_ms());
    if (tripped_.load()) {
        tripped_.store(false);
        healthy_.store(true);
        LOG_INFO("Watchdog recovered: MCU communication resumed");
    }
}

bool Watchdog::check() {
    uint64_t now = get_steady_time_ms();
    uint64_t elapsed = now - last_kick_time_ms_.load();

    if (elapsed > timeout_ms_.load()) {
        healthy_.store(false);
        if (!tripped_.exchange(true)) {
            // Edge-triggered trip event
            WatchdogTimeoutCallback cb;
            {
                std::lock_guard<std::mutex> lock(cb_mutex_);
                cb = timeout_callback_;
            }
            if (cb) {
                cb(elapsed, timeout_ms_.load());
            }
        }
        return false;
    }

    healthy_.store(true);
    return true;
}

void Watchdog::set_timeout_ms(uint32_t timeout_ms) {
    timeout_ms_.store(timeout_ms);
}

uint64_t Watchdog::get_time_since_last_kick_ms() const {
    uint64_t now = get_steady_time_ms();
    uint64_t last = last_kick_time_ms_.load();
    return (now >= last) ? (now - last) : 0;
}

void Watchdog::set_timeout_callback(WatchdogTimeoutCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    timeout_callback_ = cb;
}

} // namespace industrial
