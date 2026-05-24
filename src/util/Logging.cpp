/**
 * @file Logging.cpp
 * @brief Logging stub.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "util/Logging.h"

namespace util {

static LogLevel g_MinLevel = LogLevel::Trace;

void InitLogging() {}
void ShutdownLogging() {}
void SetLogLevel(LogLevel level) { g_MinLevel = level; }

void Log(LogLevel level, const std::string& /*fmt*/) {
    // placeholder – formatted logging in a later bead
    (void)level;
}

} // namespace util