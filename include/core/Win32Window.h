/**
 * @file core/Win32Window.h
 * @brief Win32 window and message pump implementation.
 * @details This module handles the creation and management of the main application window.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CORE_WIN32WINDOW_H
#define CORE_WIN32WINDOW_H

#ifdef _WIN32
namespace core {

class Win32Window {
public:
    Win32Window();
    ~Win32Window();

    /// @brief Runs the message pump and returns the exit code.
    int run();

private:
    // Window handle
    void* hwnd_; // Using void* to avoid including Windows.h in header
};

} // namespace core

#endif // _WIN32

#endif // CORE_WIN32WINDOW_H