/**
 * @file core/Win32Window.h
 * @brief Win32 window and message pump implementation.
 * @details This module handles the creation and management of the main application window.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CORE_WIN32WINDOW_H
#define CORE_WIN32WINDOW_H

#ifdef _WIN32

#include <functional>
#include <windows.h>

namespace core {

class Win32Window {
public:
    explicit Win32Window(std::function<void(double)> frameCallback);
    ~Win32Window();

    /// @brief Runs the message pump and returns the exit code.
    int run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void* hwnd_;
    std::function<void(double)> frameCallback_;
};

} // namespace core

#endif // _WIN32

#endif // CORE_WIN32WINDOW_H