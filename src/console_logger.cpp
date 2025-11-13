#include "console_logger.h"

console_logger::console_logger() = default;

void console_logger::log(LogLevel level, const std::string& message) {
    if (level < minLevel_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    std::string color = colorForLevel(level);
    std::string reset = "\033[0m";

    std::cout 
        << timestamp() 
        << " [" << color << levelToString(level) << reset << "] "
        << message << std::endl;
}

void console_logger::debug(const std::string& message) { log(LogLevel::Debug, message); }
void console_logger::info(const std::string& message) { log(LogLevel::Info, message); }
void console_logger::warning(const std::string& message) { log(LogLevel::Warning, message); }
void console_logger::error(const std::string& message) { log(LogLevel::Error, message); }

void console_logger::set_level(LogLevel level) {
    minLevel_ = level;
}

std::string console_logger::timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

std::string console_logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string console_logger::colorForLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "\033[36m";  // Cyan
        case LogLevel::Info:    return "\033[32m";  // Green
        case LogLevel::Warning: return "\033[33m";  // Yellow
        case LogLevel::Error:   return "\033[31m";  // Red
        default:                return "\033[0m";   // Reset
    }
}
