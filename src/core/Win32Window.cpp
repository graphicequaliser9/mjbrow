#include "core/Win32Window.h"
#include <shellscalingapi.h>
#include <cstring>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shcore.lib")

namespace core {

Win32Window::Win32Window()
    : hwnd_(nullptr)
    , hwndToolbar_(nullptr)
    , hwndUrlBar_(nullptr)
    , hInstance_(GetModuleHandle(nullptr))
    , hwndEditOldProc_(nullptr) {

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::memset(&wcex_, 0, sizeof(WNDCLASSEX));
    wcex_.cbSize = sizeof(WNDCLASSEX);
    wcex_.style = CS_HREDRAW | CS_VREDRAW;
    wcex_.lpfnWndProc = Win32Window::WndProc;
    wcex_.cbClsExtra = 0;
    wcex_.cbWndExtra = 0;
    wcex_.hInstance = hInstance_;
    wcex_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex_.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex_.lpszMenuName = nullptr;
    wcex_.lpszClassName = L"NitrogenBrowser";
    wcex_.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassEx(&wcex_);
}

Win32Window::~Win32Window() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool Win32Window::create() {
    hwnd_ = CreateWindowEx(
        0,
        L"NitrogenBrowser",
        L"Nitrogen Browser",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        nullptr,
        nullptr,
        hInstance_,
        this
    );

    if (!hwnd_) {
        return false;
    }

    createToolbar();
    createUrlBar();

    return true;
}

int Win32Window::run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Window* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Win32Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
        case WM_SIZE:
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int toolbarHeight = 40;
                int urlBarHeight = 28;
                int controlHeight = toolbarHeight + urlBarHeight;

                if (pThis->hwndToolbar_) {
                    MoveWindow(pThis->hwndToolbar_, 0, 0, rc.right, toolbarHeight, TRUE);
                }
                if (pThis->hwndUrlBar_) {
                    MoveWindow(pThis->hwndUrlBar_, 0, toolbarHeight, rc.right, urlBarHeight, TRUE);
                }
            }
            break;

        case WM_COMMAND:
            {
                int wmId = LOWORD(wParam);
                switch (wmId) {
                case 1000: // Back
                case 1001: // Forward
                case 1002: // Reload
                    break;
                default:
                    return DefWindowProc(hwnd, msg, wParam, lParam);
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Win32Window::EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return CallWindowProc(
        reinterpret_cast<WNDPROC>(hwndEditOldProc_),
        hwnd,
        msg,
        wParam,
        lParam
    );
}

void Win32Window::createToolbar() {
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&iccex);

    hwndToolbar_ = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_NODIVIDER,
        0, 0, 100, 40,
        hwnd_,
        nullptr,
        hInstance_,
        nullptr
    );

    SendMessage(hwndToolbar_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(hwndToolbar_, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));

    TBBUTTON buttons[3];
    std::memset(buttons, 0, sizeof(buttons));

    buttons[0].iBitmap = 0;
    buttons[0].idCommand = 1000;
    buttons[0].fsState = TBSTATE_ENABLED;
    buttons[0].fsStyle = BTNS_BUTTON;
    buttons[0].dwData = 0;
    buttons[0].iString = reinterpret_cast<INT_PTR>(L"Back");

    buttons[1].iBitmap = 1;
    buttons[1].idCommand = 1001;
    buttons[1].fsState = TBSTATE_ENABLED;
    buttons[1].fsStyle = BTNS_BUTTON;
    buttons[1].dwData = 0;
    buttons[1].iString = reinterpret_cast<INT_PTR>(L"Forward");

    buttons[2].iBitmap = 2;
    buttons[2].idCommand = 1002;
    buttons[2].fsState = TBSTATE_ENABLED;
    buttons[2].fsStyle = BTNS_BUTTON;
    buttons[2].dwData = 0;
    buttons[2].iString = reinterpret_cast<INT_PTR>(L"Reload");

    SendMessage(hwndToolbar_, TB_ADDBUTTONS, 3, reinterpret_cast<LPARAM>(buttons));
}

void Win32Window::createUrlBar() {
    hwndUrlBar_ = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"https://",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        0, 40, 100, 28,
        hwnd_,
        nullptr,
        hInstance_,
        nullptr
    );
}

} // namespace core