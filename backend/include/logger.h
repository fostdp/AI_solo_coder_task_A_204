#pragma once

#include <string>
#include <memory>

namespace stone_mill {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static void init(const std::string& name = "stonemill",
                     const std::string& log_file = "",
                     LogLevel level = LogLevel::INFO);

    static void set_level(LogLevel level);
    static void flush();

    static void log(LogLevel level, const char* file, int line, const char* fmt, ...);

private:
    struct Impl;
    static std::unique_ptr<Impl> impl_;
    static LogLevel level_;
    static bool initialized_;
};

}

#define LOG_TRACE(...)    ::stone_mill::Logger::log(::stone_mill::LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)    ::stone_mill::Logger::log(::stone_mill::LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)     ::stone_mill::Logger::log(::stone_mill::LogLevel::INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)     ::stone_mill::Logger::log(::stone_mill::LogLevel::WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)    ::stone_mill::Logger::log(::stone_mill::LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) ::stone_mill::Logger::log(::stone_mill::LogLevel::CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
