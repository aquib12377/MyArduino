#ifndef LOG_UTILITY_H
#define LOG_UTILITY_H

#include <Arduino.h>

// Available log levels
#define LOG_LEVEL_NONE     0
#define LOG_LEVEL_ERROR    1
#define LOG_LEVEL_WARNING  2
#define LOG_LEVEL_INFO     3
#define LOG_LEVEL_DEBUG    4

// If not defined elsewhere, set default log level to DEBUG
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

// Set this to 1 to print timestamps from millis(), 0 to skip
#ifndef LOG_PRINT_TIMESTAMP
#define LOG_PRINT_TIMESTAMP 1
#endif

inline void logMessage(uint8_t level, const char* levelStr, const char* func, int line, const String &message) {
  if (level <= LOG_LEVEL) {
    #if LOG_PRINT_TIMESTAMP
    Serial.print(millis());
    Serial.print("ms ");
    #endif
    Serial.print("[");
    Serial.print(levelStr);
    Serial.print("] ");
    Serial.print(func);
    Serial.print(":");
    Serial.print(line);
    Serial.print(" - ");
    Serial.println(message);
  }
}

#define LOG_ERROR(msg)   logMessage(LOG_LEVEL_ERROR,   "ERROR",   __FUNCTION__, __LINE__, (msg))
#define LOG_WARNING(msg) logMessage(LOG_LEVEL_WARNING, "WARNING", __FUNCTION__, __LINE__, (msg))
#define LOG_INFO(msg)    logMessage(LOG_LEVEL_INFO,    "INFO",    __FUNCTION__, __LINE__, (msg))
#define LOG_DEBUG(msg)   logMessage(LOG_LEVEL_DEBUG,   "DEBUG",   __FUNCTION__, __LINE__, (msg))

#endif // LOG_UTILITY_H
