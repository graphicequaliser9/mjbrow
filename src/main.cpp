#include <windows.h>
#include <shcore.h> // For SetProcessDpiAwareness
#include <commctrl.h> // For common controls (toolbar)
#include <string>

// Global variables
HINSTANCE hInst;
HWND hWndMain;
HWND hWndToolbar;
HWND hWndUrlBar;
int cyToolbar = 0;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HWND CreateToolbar(HWND hWndParent);

// Toolbar button IDs
enum {
    ID_BACK = 1000,
    ID_FORWARD,
    ID_RELOAD,
    ID_URLBAR
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Set DPI awareness
    // Try to use SetProcessDpiAwareness (Windows 8.1+)
    typedef HRESULT (WINAPI *SetProcessDpiAwarenessFn)(PROCESS_DPI_AWARENESS);
    SetProcessDpiAwarenessFn pSetProcessDpiAwareness =
        (SetProcessDpiAwarenessFn)GetProcAddress(GetModuleHandle(L"shcore.dll"), "SetProcessDpiAwareness");
    if (pSetProcessDpiAwareness) {
        HRESULT hr = pSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        if (FAILED(hr)) {
            // DPI awareness failed, continue anyway
        }
    } else {
        // Fallback to older method
        SetProcessDPIAware();
    }

    hInst = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES; // For toolbar
    InitCommonControlsEx(&icc);

    // Register window class
    const wchar_t CLASS_NAME[] = L"NitrogenWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

    ATOM atom = RegisterClass(&wc);
    if (atom == 0) {
        MessageBox(nullptr, L"Window registration failed!", L"Error", MB_ICONERROR);
        return 0;
    }

    // Create the main window
    hWndMain = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class
        L"Nitrogen - Lightweight Browser", // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        nullptr,       // Parent window
        nullptr,       // Menu
        hInstance,     // Instance handle
        nullptr        // Additional application data
    );

    if (hWndMain == nullptr) {
        return 0;
    }

    // Create the toolbar as a child window
    hWndToolbar = CreateToolbar(hWndMain);

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    // Resize toolbar after window is shown
    if (hWndToolbar) {
        SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
    }

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            return 0;

        case WM_SIZE:
            // Position the URL bar to the right of the toolbar buttons
            if (hWndToolbar && cyToolbar > 0) {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);

                // Position toolbar at top
                SetWindowPos(hWndToolbar, nullptr, 0, 0, rcClient.right, cyToolbar, SWP_NOZORDER);

                // Position URL bar to the right of separator (item index 3)
                RECT rcSep;
                SendMessage(hWndToolbar, TB_GETITEMRECT, 3, (LPARAM)&rcSep);

                int urlBarX = rcSep.right + 2;
                int urlBarY = 2;
                int urlBarWidth = rcClient.right - urlBarX - 2;
                int urlBarHeight = cyToolbar - 4;

                if (hWndUrlBar) {
                    SetWindowPos(hWndUrlBar, nullptr, urlBarX, urlBarY, urlBarWidth, urlBarHeight, SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BACK:
                    // TODO: Implement back navigation
                    break;
                case ID_FORWARD:
                    // TODO: Implement forward navigation
                    break;
                case ID_RELOAD:
                    // TODO: Implement reload
                    break;
                case ID_URLBAR:
                    if (HIWORD(wParam) == EN_RETURN) {
                        // TODO: Navigate to URL in edit box
                    }
                    break;
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

// Creates the toolbar with back, forward, reload buttons and a separator for URL bar
HWND CreateToolbar(HWND hWndParent) {
    // Create the toolbar window
    HWND hWndToolbar = CreateWindowEx(
        0,
        TOOLBARCLASSNAME, // Predefined toolbar class
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
        0, 0, 0, 0,
        hWndParent,
        nullptr,
        hInst,
        nullptr
    );

    if (hWndToolbar == nullptr) {
        return nullptr;
    }

    // Set the button size
    SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    // Define the toolbar buttons (back, forward, reload, separator)
    // Explicitly initialize all TBBUTTON members to avoid indeterminate values
    TBBUTTON tbButtons[] = {
        {I_IMAGENONE, ID_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Back"},
        {I_IMAGENONE, ID_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Forward"},
        {I_IMAGENONE, ID_RELOAD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Reload"},
        {I_IMAGENONE, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0} // Separator
    };

    // Add buttons to the toolbar
    SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)(sizeof(tbButtons)/sizeof(tbButtons[0])), (LPARAM)&tbButtons);

    // Get the toolbar height for later use in WM_SIZE
    RECT rc;
    GetWindowRect(hWndToolbar, &rc);
    cyToolbar = rc.bottom - rc.top;

    // Create the URL bar as an edit box child of the toolbar
    hWndUrlBar = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"about:blank",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
        0, 0, 0, 0,
        hWndToolbar,
        (HMENU)ID_URLBAR,
        hInst,
        nullptr
    );

    if (hWndUrlBar == nullptr) {
        return nullptr;
    }

    // Set default font for the edit box
    SendMessage(hWndUrlBar, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));

    return hWndToolbar;
}