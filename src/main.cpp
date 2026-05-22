/**
 * @file main.cpp
 * @brief Nitrogen browser entry point – scaffold stub.
 * @details Production code in beads 1–10.  This file exists so that CMake
 *          always has a translation unit to link.  It exercises every module
 *          include wire so that a broken include path is caught at compile
 *          time, not at runtime.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "nitrogen.h"

int main() {
    util::InitLogging();
    util::Log(util::LogLevel::Info, "Nitrogen Browser - Scaffold Initialized\n");
    util::ShutdownLogging();
    return 0;
}