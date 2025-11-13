#pragma once
#include "i_logger.h"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

class console_logger : public i_logger {
public:
    console_logger();
    void log(LogLevel level, const std::string& message) override;

    void debug(const std::string& message) override;
    void info(const std::string& message) override;
    void warning(const std::string& message) override;
    void error(const std::string& message) override;

    void set_level(LogLevel level) override;

private:
    std::mutex mutex_;
    LogLevel minLevel_{LogLevel::Debug};

    std::string timestamp();
    std::string levelToString(LogLevel level);
    std::string colorForLevel(LogLevel level);
};
