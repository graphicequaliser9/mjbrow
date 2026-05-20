#include <windows.h>
#include <windowsx.h>
#include <algorithm>

// ============================================================================
//  Nitrogen Browser — Window 0: Win32 shell
//  HWND + WndProc + DPI-aware sizing + Toolbar (Back / Forward / Reload)
//  + URL bar — loading-spinner / HTTP overlay hooks reserved for later beads
// ============================================================================

constexpr const wchar_t* kClassName = L"nitrogen.window.v1";
constexpr const wchar_t* kWinTitle  = L"Nitrogen Browser";

// Control IDs for toolbar buttons and URL edit bar
enum CtrlId {
    CTL_BUTTON_RELOAD  = 101,
    CTL_BUTTON_BACK    = 102,
    CTL_BUTTON_FORWARD = 103,
    CTL_EDIT_URL       = 201,
};

// Per-window state — heap-allocated in WM_CREATE, freed in WM_DESTROY.
// Stored in GWLP_USERDATA so every WndProc call can retrieve it in O(1).
struct BrowserWindowData {
    HWND  hwnd        = nullptr;
    HWND  hBtnBack    = nullptr;
    HWND  hBtnForward = nullptr;
    HWND  hBtnReload  = nullptr;
    HWND  hUrlEdit    = nullptr;
    int   dpi         = 96;  // current monitor DPI
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
bool      RegisterWindowClass();
HWND      CreateBrowserWindow(int nCmdShow);
void      LayoutToolbar(HWND hParent, HWND hBtnBack, HWND hBtnForward,
                        HWND hBtnReload, HWND hUrlEdit, int dpiX);

// ══════════════════════════════════════════════════════════════════════════════
//  Proportional pixel scaling from a 96-DPI baseline.
//  All geometry values in LayoutToolbar flow through this single helper.
// ══════════════════════════════════════════════════════════════════════════════
static inline int ScalePx(int px, int dpi) { return (px * dpi + 48) / 96; }

// ══════════════════════════════════════════════════════════════════════════════
//  Entry point — standard Win32 GUI message pump
// ══════════════════════════════════════════════════════════════════════════════
int APIENTRY wWinMain(_In_ HINSTANCE,
                      _In_opt_ HINSTANCE,
                      _In_ LPWSTR /*lpCmdLine*/,
                      _In_ int     nCmdShow)
{
    if (!RegisterWindowClass()) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Nitrogen",
                    MB_ICONERROR | MB_OK);
        return 1;
    }
    HWND hWnd = CreateBrowserWindow(nCmdShow);
    if (!hWnd) {
        MessageBoxW(nullptr, L"Failed to create browser window.", L"Nitrogen",
                    MB_ICONERROR | MB_OK);
        return 1;
    }
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

// ══════════════════════════════════════════════════════════════════════════════
//  Per-window DPI helper
//
//  Priority: GetDpiForWindow (Windows 10 1607+) → 96 fallback
// ══════════════════════════════════════════════════════════════════════════════
static int GetWindowDPI(HWND hWnd)
{
    static HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

    //  Windows 10 1607+ — per-window DPI from USER32
    if (hUser32) {
        using PFN_GetDpiForWindow = UINT (WINAPI *)(HWND);
        if (auto p = reinterpret_cast<PFN_GetDpiForWindow>(
                GetProcAddress(hUser32, "GetDpiForWindow")))
            return static_cast<int>(p(hWnd));
    }
    return 96;  // system-default
}

// ══════════════════════════════════════════════════════════════════════════════
//  Navigation button state manager
//
//  Stub: called from WM_CREATE on startup (both args = false → greyed out).
//  Once the HTTP layer connects it will call UpdateNavButtons with real
//  canGoBack / canGoForward values every time the back-stack changes.
// ══════════════════════════════════════════════════════════════════════════════
static void UpdateNavButtons(BrowserWindowData* pData,
                             bool canGoBack, bool canGoForward)
{
    if (!pData) return;
    if (pData->hBtnBack)
        EnableWindow(pData->hBtnBack,    canGoBack    ? TRUE : FALSE);
    if (pData->hBtnForward)
        EnableWindow(pData->hBtnForward, canGoForward ? TRUE : FALSE);
}

// ══════════════════════════════════════════════════════════════════════════════
//  DPI-scaled toolbar layout helper
//
//  Positions Back → Forward → Reload → URL bar (fills remaining width).
//  Vulgar reminder for reviewers and future maintainers:
//    --never--hardcode-pixel-values here-- call ScalePx instead.
// ══════════════════════════════════════════════════════════════════════════════
void LayoutToolbar(HWND hParent, HWND hBtnBack, HWND hBtnForward,
                   HWND hBtnReload, HWND hUrlEdit, int dpiX)
{
    const int SPACER = ScalePx(4,  dpiX);
    const int ICON   = ScalePx(24, dpiX);  // icon button width × height
    const int BAR_H  = ScalePx(36, dpiX);  // toolbar strip height
    const int EDIT_H = ScalePx(24, dpiX);  // URL bar edit height
    const int EDIT_Y = (BAR_H - EDIT_H) / 2;  // centre vertically within the bar

    int leftX = SPACER;

    if (hBtnBack) {
        SetWindowPos(hBtnBack, nullptr, leftX, (BAR_H - ICON) / 2,
                     ICON, ICON, SWP_NOZORDER);
        leftX += ICON + SPACER;
    }
    if (hBtnForward) {
        SetWindowPos(hBtnForward, nullptr, leftX, (BAR_H - ICON) / 2,
                     ICON, ICON, SWP_NOZORDER);
        leftX += ICON + SPACER;
    }
    if (hBtnReload) {
        SetWindowPos(hBtnReload, nullptr, leftX, (BAR_H - ICON) / 2,
                     ICON, ICON, SWP_NOZORDER);
        leftX += ICON + SPACER;
    }
    if (hUrlEdit) {
        RECT rc{};
        GetClientRect(hParent, &rc);
        int editW = std::max(0, rc.right - leftX - SPACER);
        SetWindowPos(hUrlEdit, nullptr, leftX, EDIT_Y, editW, EDIT_H, SWP_NOZORDER);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  Window registration
// ══════════════════════════════════════════════════════════════════════════════
bool RegisterWindowClass()
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    // TODO (DevTools bead): wc.hIcon / wc.hIconSm for taskbar branding
    return RegisterClassExW(&wc) != 0;
}

// ══════════════════════════════════════════════════════════════════════════════
//  Centred, taskbar-aware window creation
// ══════════════════════════════════════════════════════════════════════════════
HWND CreateBrowserWindow(int)
{
    // 80 % of primary monitor work area (excludes taskbar, centred on screen)
    RECT rcWork{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int workW = rcWork.right  - rcWork.left;
    int workH = rcWork.bottom - rcWork.top;
    int cx    = workW * 80 / 100;
    int cy    = workH * 80 / 100;
    int x     = rcWork.left + (workW - cx) / 2;
    int y     = rcWork.top  + (workH - cy) / 2;

    // WS_CLIPCHILDREN prevents toolbar/button content being erased when our
    // content-area WM_PAINT redraws the stretch below the strip.
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

    return CreateWindowExW(WS_EX_APPWINDOW,
                           kClassName, kWinTitle, style,
                           x, y, cx, cy,
                           nullptr, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
}

// ══════════════════════════════════════════════════════════════════════════════
//  Window procedure
//
//  ┌─── Handled messages ────────────────────────────────────────────────────┐
//  │ WM_CREATE        Allocate BrowserWindowData; build toolbar strip          │
//  │ WM_SIZE          Re-layout toolbar on client resize                      │
//  │ WM_GETDPISCALEDSIZE  Accept proposed window rect (Win10 1607+) ++       │
//  │ WM_DPICHANGED    Accept proposed rect, store new DPI, WM_SIZE fires later│
//  │ WM_GOBACK        Navigation stub (HTTP layer replaces this)              │
//  │ WM_GOFORWARD     Navigation stub (HTTP layer replaces this)              │
//  │ WM_RELOAD        Navigation stub (HTTP layer replaces this)              │
//  │ WM_PAINT         Placeholder fill (CSS3 paint engine replaces this)      │
//  │ WM_DESTROY       Free BrowserWindowData; post quit message                │
//  └──────────────────────────────────────────────────────────────────────────┘
// ══════════════════════════════════════════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HINSTANCE g_hInst = GetModuleHandleW(nullptr);

    BrowserWindowData* pData = reinterpret_cast<BrowserWindowData*>(
        GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg)
    {
    // ── WM_CREATE ─────────────────────────────────────────────────────────────
    case WM_CREATE: {
        // Allocate per-window state from the process heap.
        // HeapAlloc avoids bringing in <new> just for BrowserWindowData.
        BrowserWindowData* pNew = static_cast<BrowserWindowData*>(HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BrowserWindowData)));
        if (!pNew) return -1;

        pNew->hwnd = hWnd;
        pNew->dpi  = GetWindowDPI(hWnd);

        // ── Toolbar container strip (top edge of the client area) ────────────
        HWND hToolbar = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,  // owner-draw reserved for embed+Paint
            0, 0, 0, 0,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9001)),
            g_hInst, nullptr);

        // ── Navigation buttons ────────────────────────────────────────────────
        // Code-point labels (← → ↻) keep the binary dependency-free; a CSS3
        // sprite or vector icon replaces them in a later milestone.
        pNew->hBtnBack = CreateWindowExW(
            0, L"BUTTON", L"\x2190",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 0, 0, hToolbar,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(CTL_BUTTON_BACK)),
            g_hInst, nullptr);

        pNew->hBtnForward = CreateWindowExW(
            0, L"BUTTON", L"\x2192",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 0, 0, hToolbar,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(CTL_BUTTON_FORWARD)),
            g_hInst, nullptr);

        pNew->hBtnReload = CreateWindowExW(
            0, L"BUTTON", L"\x21BB",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 0, 0, hToolbar,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(CTL_BUTTON_RELOAD)),
            g_hInst, nullptr);

        // ── URL / search bar ──────────────────────────────────────────────────
        pNew->hUrlEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"https://",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
            0, 0, 0, 0, hToolbar,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(CTL_EDIT_URL)),
            g_hInst, nullptr);

        // DPI-scaled Segoe UI for all toolbar controls (kept below toolbar h)
        {
            HFONT hFnt = CreateFontW(
                -MulDiv(11, pNew->dpi, 72),  // ~11 pt at system DPI
                0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (hFnt) {
                SendMessageW(pNew->hBtnBack,    WM_SETFONT, reinterpret_cast<WPARAM>(hFnt), TRUE);
                SendMessageW(pNew->hBtnForward, WM_SETFONT, reinterpret_cast<WPARAM>(hFnt), TRUE);
                SendMessageW(pNew->hBtnReload,  WM_SETFONT, reinterpret_cast<WPARAM>(hFnt), TRUE);
                SendMessageW(pNew->hUrlEdit,    WM_SETFONT, reinterpret_cast<WPARAM>(hFnt), TRUE);
            }
        }

        // No navigation history on startup → grey out Back / Forward
        UpdateNavButtons(pNew, false, false);

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pNew));
        pData = pNew;
        return 0;
    }

    // ── WM_SIZE ───────────────────────────────────────────────────────────────
    case WM_SIZE: {
        if (pData) {
            LayoutToolbar(pData->hwnd,
                          pData->hBtnBack, pData->hBtnForward,
                          pData->hBtnReload, pData->hUrlEdit,
                          pData->dpi);
        }
        return 0;
    }

    // ── WM_GETDPISCALEDSIZE  (Win10 1607+) ────────────────────────────────────
    case WM_GETDPISCALEDSIZE:
        // Accept the system-suggested minimum window size; we set no lower bound.
        CopyRect(reinterpret_cast<LPRECT>(lParam),
                 reinterpret_cast<const RECT*>(wParam));
        return TRUE;

    // ── WM_DPICHANGED  (Win8.1 / Win10 1607+) ─────────────────────────────────
    //  lParam = proposed new window rect for the new monitor DPI.
    //  wParam = HIWORD = new DPI, LOWORD = old DPI.
    case WM_DPICHANGED: {
        if (pData) {
            RECT* pNewRect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hWnd, nullptr,
                         pNewRect->left, pNewRect->top,
                         pNewRect->right  - pNewRect->left,
                         pNewRect->bottom - pNewRect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            pData->dpi = HIWORD(wParam);
        }
        return 0;
    }

    // ── WM_COMMAND  (toolbar buttons + URL bar) ────────────────────────────────
    case WM_COMMAND: {
        int id = LOWORD(wParam);

        switch (id) {
        case CTL_EDIT_URL:
            // TODO [HTTP layer]: parse URL bar on EN_RETURN / EN_CHANGE and
            // dispatch a GET request via the navigation engine.
            break;

        case CTL_BUTTON_RELOAD:
            SendMessageW(hWnd, WM_RELOAD, 0, 0);
            return 0;

        case CTL_BUTTON_BACK:
            SendMessageW(hWnd, WM_GOBACK, 0, 0);
            return 0;

        case CTL_BUTTON_FORWARD:
            SendMessageW(hWnd, WM_GOFORWARD, 0, 0);
            return 0;
        }
        break;
    }

    // ── WM_GOBACK ─────────────────────────────────────────────────────────────
    //  Stub: replaced by HTTP/navigation layer once that bead lands.
    //  Grep "stub" to find every not-yet-wired call-site.
    case WM_GOBACK:
        MessageBoxW(hWnd, L"Back \x2190 — stub; not connected to HTTP navigation",
                    L"Navigate", MB_OK | MB_ICONINFORMATION);
        return 0;

    // ── WM_GOFORWARD ──────────────────────────────────────────────────────────
    case WM_GOFORWARD:
        MessageBoxW(hWnd, L"Forward \x2192 — stub; not connected to HTTP navigation",
                    L"Navigate", MB_OK | MB_ICONINFORMATION);
        return 0;

    // ── WM_RELOAD ─────────────────────────────────────────────────────────────
    case WM_RELOAD:
        MessageBoxW(hWnd, L"Reload \x21BB — stub; not connected to HTTP navigation",
                    L"Navigate", MB_OK | MB_ICONINFORMATION);
        return 0;

    // ── WM_PAINT ──────────────────────────────────────────────────────────────
    //  Solid fill is a deliberate no-op.  The CSS3 painting engine owns the real
    //  rendering path once the HTML5 parser + CSS cascade beads wire it in.
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hWnd, &ps);
        FillRect(ps.hdc, &ps.rcPaint,
                 reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        EndPaint(hWnd, &ps);
        return 0;
    }

    // ── WM_DESTROY ────────────────────────────────────────────────────────────
    case WM_DESTROY: {
        if (pData) {
            HeapFree(GetProcessHeap(), 0, pData);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    }   // end switch

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
