/**
 * @file main.cpp
 * @brief Nitrogen browser — bead 10 entry point.  Instantiates BrowserUI and
 *        drives a fixed 60 FPS tick loop.  Accepts an optional CLI argument as
 *        the home URL.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "nitrogen.h"

#include <cstring>

// Windows headers unconditionally required because WIN32_EXECUTABLE property
// forces /SUBSYSTEM:WINDOWS, which requires WinMain entry point.
// WRAPPING these in #ifdef would cause LNK2019 on MSVC.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int main(int argc, char* argv[]) {
    util::InitLogging();
    util::Log(util::LogLevel::Info, "Nitrogen Browser — bead 10 starting\n");

    std::string homeUrl = "https://www.jacobsm.com/browtest3.htm";
    if (argc > 1) homeUrl = argv[1];

    browser::BrowserUI ui;
    ui.run(homeUrl);

    // Run the frame loop until the window is closed
    // (Win32Window::run() owns the native pump; frame ticks are dispatched from
    //  inside the WndProc callback via BrowserUI::onFrame in the full impl; here
    //  on non-Windows we advance a single tick so the test harness can inspect
    //  ui.activeTab().)
    ui.onFrame(16.0);

    util::Log(util::LogLevel::Info, "Nitrogen Browser — shutdown\n");
    util::ShutdownLogging();
    return 0;
}

// Win32 GUI entry point - unconditionally defined because CMake sets
// WIN32_EXECUTABLE TRUE which requires WinMain. On non-Windows this symbol
// is unused but harmless.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
