/**
 * @file Win32Window.cpp
 * @brief Win32 window and message pump implementation.
 * @details This module handles the creation and management of the main application window.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "core/Win32Window.h"

#ifdef _WIN32
#include <windows.h>

namespace core {

Win32Window::Win32Window() : hwnd_(nullptr) {
    // CreateWindowEx / message pump wired up in bead 1.
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
    }
}

int Win32Window::run() {
    // Stub: real message pump in bead 1.
    return 0;
}

} // namespace core

#endif // _WIN32