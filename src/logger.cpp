#include "logger.h"

LogLevel Logger::currentLevel = LOG_LEVEL_INFO;

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

LogLevel Logger::getLevel() {
    return currentLevel;
}

void Logger::printLog(const char* levelStr, const char* module, const char* format, va_list args) {
    if (Serial.availableForWrite() < 32) return; // Drop logs if UART is blocked/full to prevent freezing
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    Serial.printf("[%s] [%s] %s\n", levelStr, module, buffer);
}

void Logger::error(const char* module, const char* format, ...) {
    if (currentLevel < LOG_LEVEL_ERROR) return;
    va_list args;
    va_start(args, format);
    printLog("ERR ", module, format, args);
    va_end(args);
}

void Logger::warn(const char* module, const char* format, ...) {
    if (currentLevel < LOG_LEVEL_WARN) return;
    va_list args;
    va_start(args, format);
    printLog("WARN", module, format, args);
    va_end(args);
}

void Logger::info(const char* module, const char* format, ...) {
    if (currentLevel < LOG_LEVEL_INFO) return;
    va_list args;
    va_start(args, format);
    printLog("INFO", module, format, args);
    va_end(args);
}

void Logger::debug(const char* module, const char* format, ...) {
    if (currentLevel < LOG_LEVEL_DEBUG) return;
    va_list args;
    va_start(args, format);
    printLog("DBUG", module, format, args);
    va_end(args);
}
