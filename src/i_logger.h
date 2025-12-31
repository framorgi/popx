#pragma once
#include <string>

enum class LogLevel { Debug, Info, Warning, Error };

class ILogger {
  public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, const std::string& message) = 0;

    virtual void debug(const std::string& message) = 0;
    virtual void info(const std::string& message) = 0;
    virtual void warning(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;

    virtual void set_level(LogLevel level) = 0;
};
