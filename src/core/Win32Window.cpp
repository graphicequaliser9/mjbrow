/**
 * @file Win32Window.cpp
 * @brief Win32 window and message pump implementation.
 * @details This module handles the creation and management of the main application window.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "core/Win32Window.h"

#ifdef _WIN32
#include <windows.h>

#include <utility>

namespace core {
namespace {
constexpr const wchar_t* kClassName = L"NitrogenBrowserWindowClass";
constexpr const wchar_t* kWindowTitle = L"Nitrogen Browser";
constexpr UINT_PTR kFrameTimerId = 1;
}

Win32Window::Win32Window(std::function<void(double)> frameCallback)
    : hwnd_(nullptr)
    , frameCallback_(std::move(frameCallback)) {
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
    }
}

int Win32Window::run() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    windowClass.lpszClassName = kClassName;

    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 1;
    }

    hwnd_ = CreateWindowExW(
        0,
        kClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        800,
        nullptr,
        nullptr,
        windowClass.hInstance,
        this);

    if (!hwnd_) {
        return 1;
    }

    SetTimer(static_cast<HWND>(hwnd_), kFrameTimerId, 16, nullptr);
    ShowWindow(static_cast<HWND>(hwnd_), SW_SHOWNORMAL);
    UpdateWindow(static_cast<HWND>(hwnd_));

    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            return 1;
        }
        if (result == 0) {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        window = static_cast<Win32Window*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_TIMER:
        if (wParam == kFrameTimerId && window && window->frameCallback_) {
            window->frameCallback_(16.0);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paintStruct{};
        BeginPaint(hwnd, &paintStruct);
        if (window && window->frameCallback_) {
            window->frameCallback_(16.0);
        }
        EndPaint(hwnd, &paintStruct);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (window) {
            KillTimer(hwnd, kFrameTimerId);
            window->hwnd_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace core

#endif // _WIN32