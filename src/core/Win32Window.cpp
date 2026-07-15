/**
 * @file Win32Window.cpp
 * @brief Win32 window and message pump implementation.
 * @details Creates a WS_OVERLAPPEDWINDOW with a menu bar (File / Navigation), 
 *          runs a 60-frame message pump, and dispatches WM_PAINT and
 *          WM_SIZE to BrowserUI so frames are ticked and the viewport stays in
 *          sync with the client area.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "core/Win32Window.h"
#include "browser/BrowserUI.h"

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

namespace core {

namespace {

constexpr LPCWSTR kWndClassName = L"NitrogenBrowserWindow";
constexpr LPCWSTR kWndTitle     = L"Nitrogen Browser";

LRESULT dispatchBrowserCommand(Win32Window* self, UINT cmdId) {
    browser::BrowserUI* ui = self->ui_;
    if (!ui) return 0;

    switch (cmdId) {
    case Win32Window::ID_BACK:
        if (auto* tab = ui->activeTab()) tab->goBack();
        break;
    case Win32Window::ID_FORWARD:
        if (auto* tab = ui->activeTab()) tab->goForward();
        break;
    case Win32Window::ID_RELOAD:
        if (auto* tab = ui->activeTab()) tab->goReload();
        break;
    case 200: // IDM_EXIT
        PostQuitMessage(0);
        break;
    case 201: // IDM_FILE_NEW
        ui->newTab();
        break;
    case 202: // IDM_ABOUT
        MessageBoxW(nullptr, L"Nitrogen Browser - Lightweight Windows Browser", L"About", MB_OK);
        break;
    default:
        break;
    }
    return 0;
}

} // anonymous namespace

Win32Window::Win32Window(browser::BrowserUI* ui)
    : ui_(ui)
    , hwnd_(nullptr)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.hInstance     = hInst;
    wc.lpszClassName = kWndClassName;
    wc.lpfnWndProc   = &Win32Window::WndProc;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    HMENU menuBar = CreateMenu();
    
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, 200, L"E&xit");
    AppendMenuW(fileMenu, MF_STRING, 201, L"&New Tab");
    AppendMenuW(fileMenu, MF_STRING, 202, L"&About");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");

    HMENU navMenu  = CreatePopupMenu();
    AppendMenuW(navMenu, MF_STRING, static_cast<UINT_PTR>(ID_BACK),    L"&Back");
    AppendMenuW(navMenu, MF_STRING, static_cast<UINT_PTR>(ID_FORWARD), L"&Forward");
    AppendMenuW(navMenu, MF_STRING, static_cast<UINT_PTR>(ID_RELOAD),  L"&Reload");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(navMenu), L"&Navigation");

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    int   w = 1280, h = 800;

    hwnd_ = CreateWindowExW(
        0,
        kWndClassName,
        kWndTitle,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr,
        menuBar,
        hInst,
        static_cast<LPVOID>(this)
    );

    if (hwnd_) {
        ShowWindow(static_cast<HWND>(hwnd_), SW_SHOWNORMAL);
        UpdateWindow(static_cast<HWND>(hwnd_));
    }
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
}

int Win32Window::run() {
    MSG msg{};
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return static_cast<int>(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (hwnd_ && ui_) {
            ui_->onFrame(16.0);
        }

        Sleep(16);
    }
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
    case WM_SIZE:
        if (self) self->onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (self) {
            self->onPaint(hdc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        return dispatchBrowserCommand(self, static_cast<UINT>(LOWORD(wParam)));

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void Win32Window::onSize(int w, int h) {
    if (!ui_) return;
    if (auto* tab = ui_->activeTab()) {
        if (auto* vw = tab->webView()) {
            vw->width  = w;
            vw->height = h;
        }
    }
}

void Win32Window::onPaint(HDC hdc, const RECT& rcPaint) {
    if (!ui_) return;
    ui_->renderPage(hdc, rcPaint);
}

} // namespace core

#endif // _WIN32