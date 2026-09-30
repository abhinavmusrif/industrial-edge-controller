#include "logger.hpp"
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace industrial {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void Logger::init(const std::string& log_file_path, LogLevel min_level, bool echo_console) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = min_level;
    echo_console_ = echo_console;

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    file_stream_.open(log_file_path, std::ios::out | std::ios::app);
    initialized_ = true;
}

std::string Logger::get_iso8601_timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);

    std::tm bt{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&bt, &timer);
#else
    gmtime_r(&timer, &bt);
#endif

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << 'Z';
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string timestamp = get_iso8601_timestamp();
    const char* lvl_str = level_to_string(level);

    std::string line = timestamp + " " + lvl_str + " " + message;

    if (file_stream_.is_open()) {
        file_stream_ << line << "\n";
        file_stream_.flush();
    }

    if (echo_console_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << line << "\n";
        } else {
            std::cout << line << "\n";
        }
    }
}

LogLevel Logger::parse_level(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "DEBUG") return LogLevel::DEBUG;
    if (s == "INFO") return LogLevel::INFO;
    if (s == "WARN" || s == "WARNING") return LogLevel::WARN;
    if (s == "ERROR") return LogLevel::ERROR;
    if (s == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::INFO;
}

const char* Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO ";
        case LogLevel::WARN:     return "WARN ";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRIT ";
        default:                 return "INFO ";
    }
}

} // namespace industrial
