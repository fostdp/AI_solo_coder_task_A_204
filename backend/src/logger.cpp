#include "logger.h"

#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#endif

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <mutex>
#include <iomanip>
#include <sstream>

namespace stone_mill {

struct Logger::Impl {
#ifdef USE_SPDLOG
    std::shared_ptr<spdlog::logger> logger;
#endif
    std::mutex mtx;
    std::string name;
    std::string log_file;
};

std::unique_ptr<Logger::Impl> Logger::impl_ = nullptr;
LogLevel Logger::level_ = LogLevel::INFO;
bool Logger::initialized_ = false;

namespace {

const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRIT ";
    }
    return "?????";
}

const char* level_color(LogLevel l) {
    switch (l) {
        case LogLevel::TRACE: return "\033[36m";
        case LogLevel::DEBUG: return "\033[34m";
        case LogLevel::INFO:  return "\033[32m";
        case LogLevel::WARN:  return "\033[33m";
        case LogLevel::ERROR: return "\033[31m";
        case LogLevel::CRITICAL: return "\033[1;31m";
    }
    return "";
}

constexpr const char* RESET = "\033[0m";

std::string now_str() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

}

void Logger::init(const std::string& name,
                  const std::string& log_file,
                  LogLevel level) {
    if (initialized_) return;
    impl_ = std::make_unique<Impl>();
    impl_->name = name;
    impl_->log_file = log_file;
    level_ = level;

#ifdef USE_SPDLOG
    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        if (!log_file.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, 10 * 1024 * 1024, 5));
        }
        impl_->logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        impl_->logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        spdlog::level::level_enum spd_lvl = spdlog::level::info;
        switch (level) {
            case LogLevel::TRACE:    spd_lvl = spdlog::level::trace; break;
            case LogLevel::DEBUG:    spd_lvl = spdlog::level::debug; break;
            case LogLevel::INFO:     spd_lvl = spdlog::level::info; break;
            case LogLevel::WARN:     spd_lvl = spdlog::level::warn; break;
            case LogLevel::ERROR:    spd_lvl = spdlog::level::err; break;
            case LogLevel::CRITICAL: spd_lvl = spdlog::level::critical; break;
        }
        impl_->logger->set_level(spd_lvl);
        impl_->logger->flush_on(spdlog::level::warn);
        initialized_ = true;
        return;
    } catch (const std::exception& e) {
        std::cerr << "[Logger] spdlog init failed, fallback to iostream: " << e.what() << std::endl;
    }
#endif

    initialized_ = true;
}

void Logger::set_level(LogLevel level) {
    level_ = level;
#ifdef USE_SPDLOG
    if (initialized_ && impl_ && impl_->logger) {
        spdlog::level::level_enum spd_lvl = spdlog::level::info;
        switch (level) {
            case LogLevel::TRACE:    spd_lvl = spdlog::level::trace; break;
            case LogLevel::DEBUG:    spd_lvl = spdlog::level::debug; break;
            case LogLevel::INFO:     spd_lvl = spdlog::level::info; break;
            case LogLevel::WARN:     spd_lvl = spdlog::level::warn; break;
            case LogLevel::ERROR:    spd_lvl = spdlog::level::err; break;
            case LogLevel::CRITICAL: spd_lvl = spdlog::level::critical; break;
        }
        impl_->logger->set_level(spd_lvl);
    }
#endif
}

void Logger::flush() {
#ifdef USE_SPDLOG
    if (initialized_ && impl_ && impl_->logger) {
        impl_->logger->flush();
    }
#endif
}

void Logger::log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (!initialized_) init();
    if (level < level_) return;

    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

#ifdef USE_SPDLOG
    if (impl_ && impl_->logger) {
        switch (level) {
            case LogLevel::TRACE:    impl_->logger->trace("{}", buf); break;
            case LogLevel::DEBUG:    impl_->logger->debug("{}", buf); break;
            case LogLevel::INFO:     impl_->logger->info("{}", buf); break;
            case LogLevel::WARN:     impl_->logger->warn("{}", buf); break;
            case LogLevel::ERROR:    impl_->logger->error("{}", buf); break;
            case LogLevel::CRITICAL: impl_->logger->critical("{}", buf); break;
        }
        return;
    }
#endif

    std::lock_guard<std::mutex> lk(impl_ ? impl_->mtx : *(new std::mutex()));
    auto& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;
    out << "[" << now_str() << "] "
        << level_color(level) << "[" << level_name(level) << "]" << RESET << " "
        << buf;
    if (level >= LogLevel::WARN) {
        out << "  (" << file << ":" << line << ")";
    }
    out << std::endl;
}

}
