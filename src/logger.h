#pragma once
#include <Arduino.h>
#include <stdarg.h>

enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel getLevel();
    
    static void error(const char* module, const char* format, ...);
    static void warn(const char* module, const char* format, ...);
    static void info(const char* module, const char* format, ...);
    static void debug(const char* module, const char* format, ...);

private:
    static LogLevel currentLevel;
    static void printLog(const char* levelStr, const char* module, const char* format, va_list args);
};
