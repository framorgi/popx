#pragma once
#include "i_logger.h"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

class ConsoleLogger : public ILogger {
public:
    ConsoleLogger();
    void log(LogLevel level, const std::string& message) override;

    void debug(const std::string& message) override;
    void info(const std::string& message) override;
    void warning(const std::string& message) override;
    void error(const std::string& message) override;

    void set_level(LogLevel level) override;

private:
    std::mutex mutex_;
    LogLevel min_level_{LogLevel::Debug};

    std::string timestamp();
    std::string level_to_string(LogLevel level);
    std::string color_for_level(LogLevel level);
};
