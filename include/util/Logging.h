/**
 * @file util/Logging.h
 * @brief Logging facility used across all modules.
 * @details Lightweight, thread-safe logging stub.  Level filtering implemented
 *          in later beads; for now all messages always print to stdout.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef UTIL_LOGGING_H
#define UTIL_LOGGING_H

#include <string>

namespace util {

enum class LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Trace,
};

/// @brief Initialises the logging subsystem.
void InitLogging();

/// @brief Shuts down the logging subsystem.
void ShutdownLogging();

/// @brief Sets the minimum log level that will be emitted.
/// @param level Minimum level; messages below this are discarded.
void SetLogLevel(LogLevel level);

/// @brief Logs a formatted message at the given severity.
/// @param level Severity of the message.
/// @param fmt  printf-style format string.
/// @param ...  Format arguments.
void Log(LogLevel level, const std::string& fmt);

} // namespace util

#endif // UTIL_LOGGING_H