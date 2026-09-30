#ifndef INDUSTRIAL_WATCHDOG_HPP
#define INDUSTRIAL_WATCHDOG_HPP

#include "common.hpp"
#include <atomic>
#include <functional>
#include <mutex>

namespace industrial {

using WatchdogTimeoutCallback = std::function<void(uint64_t elapsed_ms, uint64_t timeout_ms)>;

class Watchdog {
public:
    explicit Watchdog(uint32_t timeout_ms = 2000);
    ~Watchdog() = default;

    // Pet / Kick the watchdog whenever a valid frame arrives from MCU
    void kick();

    // Check if watchdog has expired
    bool check();

    void set_timeout_ms(uint32_t timeout_ms);
    uint32_t get_timeout_ms() const { return timeout_ms_.load(); }
    
    bool is_healthy() const { return healthy_.load(); }
    uint64_t get_time_since_last_kick_ms() const;

    void set_timeout_callback(WatchdogTimeoutCallback cb);

private:
    std::atomic<uint32_t> timeout_ms_{2000};
    std::atomic<uint64_t> last_kick_time_ms_{0};
    std::atomic<bool> healthy_{true};
    std::atomic<bool> tripped_{false};

    mutable std::mutex cb_mutex_;
    WatchdogTimeoutCallback timeout_callback_;
};

} // namespace industrial

#endif // INDUSTRIAL_WATCHDOG_HPP
