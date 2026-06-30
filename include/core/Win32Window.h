/**
 * @file core/Win32Window.h
 * @brief Win32 window and message pump implementation.
 * @details This module handles the creation and management of the main application
 *          window, the native message pump, and dispatching input to BrowserUI.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CORE_WIN32WINDOW_H
#define CORE_WIN32WINDOW_H

#ifdef _WIN32

namespace browser { class BrowserUI; }

namespace core {

class Win32Window {
public:
    static constexpr UINT ID_BACK    = 100;
    static constexpr UINT ID_FORWARD = 101;
    static constexpr UINT ID_RELOAD  = 102;

    explicit Win32Window(browser::BrowserUI* ui);
    ~Win32Window();

    int run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onSize(int w, int h);
    void onPaint(const RECT& rcPaint);

    browser::BrowserUI* ui_;
    void*                hwnd_;
};

} // namespace core

#endif // _WIN32

#endif // CORE_WIN32WINDOW_H