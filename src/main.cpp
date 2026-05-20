#include <windows.h>
#include <shcore.h> // For SetProcessDpiAwareness
#include <commctrl.h> // For common controls (toolbar)
#include <string>

// Global variables
HINSTANCE hInst;
HWND hWndMain;
HWND hWndToolbar;
HWND hWndUrlBar;

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
        pSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
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

    RegisterClass(&wc);

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
            // Toolbar already created in WinMain after window creation
            return 0;

        case WM_SIZE:
            // Resize the toolbar and URL bar
            if (hWndToolbar) {
                // Toolbar height is typically fixed, we'll let it auto-size
                SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
                
                // Position the URL bar to the right of the toolbar buttons
                RECT rcToolbar;
                GetClientRect(hWndToolbar, &rcToolbar);
                
                // Get the rectangle of the separator (item index 3) to position the URL bar
                // Items: 0=Back, 1=Forward, 2=Reload, 3=Separator
                RECT rcSep;
                SendMessage(hWndToolbar, TB_GETITEMRECT, 3, (LPARAM)&rcSep);
                
                // Position URL bar: start after separator, full height of toolbar
                int urlBarX = rcSep.right + 2; // Small padding after separator
                int urlBarY = 2; // Small top padding
                int urlBarWidth = rcToolbar.right - urlBarX - 2; // Right padding
                int urlBarHeight = rcToolbar.bottom - 4; // Top and bottom padding
                
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
    TBBUTTON tbButtons[] = {
        {0, ID_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Back"},
        {1, ID_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Forward"},
        {2, ID_RELOAD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Reload"},
        {3, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0} // Separator
    };

    // Add buttons to the toolbar
    SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)(sizeof(tbButtons)/sizeof(tbButtons[0])), (LPARAM)&tbButtons);

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