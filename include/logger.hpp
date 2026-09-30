#ifndef INDUSTRIAL_LOGGER_HPP
#define INDUSTRIAL_LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>

#ifdef ERROR
#undef ERROR
#endif

namespace industrial {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& log_file_path, LogLevel min_level = LogLevel::INFO, bool echo_console = true);
    void log(LogLevel level, const std::string& message);

    void debug(const std::string& msg)   { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg)    { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg)    { log(LogLevel::WARN, msg); }
    void error(const std::string& msg)   { log(LogLevel::ERROR, msg); }
    void critical(const std::string& msg){ log(LogLevel::CRITICAL, msg); }

    static LogLevel parse_level(const std::string& str);
    static const char* level_to_string(LogLevel level);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string get_iso8601_timestamp();

    std::ofstream file_stream_;
    LogLevel min_level_{LogLevel::INFO};
    bool echo_console_{true};
    std::mutex mutex_;
    bool initialized_{false};
};

// Convenience macros
#define LOG_DEBUG(msg)    ::industrial::Logger::instance().debug(msg)
#define LOG_INFO(msg)     ::industrial::Logger::instance().info(msg)
#define LOG_WARN(msg)     ::industrial::Logger::instance().warn(msg)
#define LOG_ERROR(msg)    ::industrial::Logger::instance().error(msg)
#define LOG_CRITICAL(msg) ::industrial::Logger::instance().critical(msg)

} // namespace industrial

#endif // INDUSTRIAL_LOGGER_HPP
