#include "console_logger.h"

ConsoleLogger::ConsoleLogger() = default;

void ConsoleLogger::log(LogLevel level, const std::string& message) {
    if (level < min_level_)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    std::string color = color_for_level(level);
    std::string reset = "\033[0m";

    std::cout << timestamp() << " [" << color << level_to_string(level) << reset << "] " << message << std::endl;
}

void ConsoleLogger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}
void ConsoleLogger::info(const std::string& message) {
    log(LogLevel::Info, message);
}
void ConsoleLogger::warning(const std::string& message) {
    log(LogLevel::Warning, message);
}
void ConsoleLogger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void ConsoleLogger::set_level(LogLevel level) {
    min_level_ = level;
}

std::string ConsoleLogger::timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

std::string ConsoleLogger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string ConsoleLogger::color_for_level(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "\033[36m"; // Cyan
        case LogLevel::Info:
            return "\033[32m"; // Green
        case LogLevel::Warning:
            return "\033[33m"; // Yellow
        case LogLevel::Error:
            return "\033[31m"; // Red
        default:
            return "\033[0m"; // Reset
    }
}
