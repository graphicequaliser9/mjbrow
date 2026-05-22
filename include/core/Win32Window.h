#ifndef CORE_WIN32WINDOW_H
#define CORE_WIN32WINDOW_H

#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>

namespace core {

class Win32Window {
public:
    Win32Window();
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool create();
    int run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void createToolbar();
    void createUrlBar();

    HWND hwnd_;
    HWND hwndToolbar_;
    HWND hwndUrlBar_;
    HINSTANCE hInstance_;
    WNDCLASSEX wcex_;
    WNDPROC hwndEditOldProc_;
};

} // namespace core

#else // _WIN32

namespace core {

class Win32Window {
public:
    Win32Window() {}
    ~Win32Window() {}

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool create() { return false; }
    int run() { return 0; }
};

} // namespace core

#endif // _WIN32

#endif // CORE_WIN32WINDOW_H