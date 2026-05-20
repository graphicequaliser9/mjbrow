# Nitrogen — Lightweight Windows Browser

A native Windows web browser built with **C++17** and the **Win32 API**.
Targets **MSVC** (Visual Studio 2019 / 2022) and **MinGW-w64** from a single CMake
build file.

---

## Project layout

```
.
├── CMakeLists.txt          # Build file (CMake ≥ 3.10, C++17, WIN32 subsystem)
├── README.md
├── include/                # Public headers
├── src/
│   └── main.cpp            # Window 0: Win32 HWND, WndProc, toolbar, URL bar
├── tests/                  # Unit / integration tests
├── third_party/            # Vendored dependencies (unpacked here only)
└── docs/                   # Architecture notes, ADRs, RFCs
```

---

## Current milestone — 0 · Win32 shell

`src/main.cpp` implements the native windowing layer in a single translation unit.

| Feature | Status |
|---|---|
| **HWND** creation (centred, SPI\_GETWORKAREA) | ✅ |
| **WndProc** full switch | ✅ |
| **DPI-aware** (`WM_GETDPISCALEDSIZE`, `WM_DPICHANGED`, `GetDpiForWindow`) | ✅ |
| **Toolbar strip** (`SS_OWNERDRAW` placeholder → CSS owner-draw later) | ✅ |
| **Back / Forward / Reload** buttons  (← → ↻ code-point labels) | ✅ |
| **URL / search bar** (`EDIT` + `ES_AUTOHSCROLL`, `CTL_EDIT_URL` ID) | ✅ |
| Toolbar layout helper  (`LayoutToolbar`, `ScalePx`) | ✅ |
| Nav button state manager  (`UpdateNavButtons` grey-out on startup) | ✅ |
| HTTP / navigation engine connection | 🚧 stub — protocol todo below |
| CSS3 paint surface (`WM_PAINT` → solid fill → **CSS engine** wired later) | 🚧 stub |
| Loading-spinner toolbar overlay | 🚧 reserved hook (button enum + layout slot ready) |

### Navigation stubs

`WM_GOBACK`, `WM_GOFORWARD`, and `WM_RELOAD` currently call `MessageBoxW` so a
reviewer can find every wiring gap by grepping for `stub`.  The [HTTP networking
layer bead](dd1f8b7d) replaces every `stub` call-site with a real
`WinHTTP`/`WinInet` dispatch; it also calls `UpdateNavButtons` with the new
`canGoBack` / `canGoForward` state.

---

## Build prerequisites

| Requirement | Minimum |
|---|---|
| CMake | 3.10 |
| MSVC | VS 2019 (v142) or VS 2022 (v143) |
| MinGW-w64 | 8.1+ (`x86_64-w64-mingw32-g++`) |
| Windows SDK | 10.0.17763.0 (1809; needed for `GetDpiForWindow`) |

---

## Building

### MSVC (Developer PowerShell)

```powershell
# Configure
cmake -B build -S . -A x64 -G "Visual Studio 17 2022"

# Build Release
cmake --build build --config Release

# Build Debug
cmake -B build-debug -S . -A x64 -G "Visual Studio 17 2022"
cmake --build build-debug --config Debug
```

### MinGW-w64 (MSYS2 / Git Bash)

```bash
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc

cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
cmake --build build --config Release
```

### CI cross-compile smoke-test (Linux/macOS, no link)

```bash
cmake -B build -S .  -DCMAKE_CXX_COMPILER=g++
cmake --build build       # source check only — Win32 headers absent on host
```

---

## Run

```powershell
# MSVC
& .\build\Release\NitrogenBrowser.exe

# MinGW
& .\build\NitrogenBrowser.exe
```

---

## Glossary of patterns

### DPI scaling

```
ScalePx(base_px, current_dpi)

  ScalePx(24,  96)  →  24   // 100 %
  ScalePx(24,  144) →  36   // 150 %
  ScalePx(24,  192) →  48   // 200 %
```

All geometry in `LayoutToolbar` flows through `ScalePx`, so the toolbar
re-layouts automatically when `WM_DPICHANGED` fires with a new DPI in
`HIWORD(wParam)`.

### Message dispatch sketch

```
User clicks a toolbar button  →  WM_COMMAND (id=CTRL_…)
      │
      ├─ CTL_BUTTON_BACK       → SendMessage(hWnd, WM_GOBACK, …)
      ├─ CTL_BUTTON_FORWARD    → SendMessage(hWnd, WM_GOFORWARD, …)
      ├─ CTL_BUTTON_RELOAD     → SendMessage(hWnd, WM_RELOAD, …)
      └─ CTL_EDIT_URL + Return → stub → MessageBoxW (→ HTTP layer later)

Client area resized            → WM_SIZE → LayoutToolbar(dpiX, children…)
DPI monitor changed            → WM_DPICHANGED → SetWindowPos(proposedRect);
                                → WM_SIZE → LayoutToolbar(newDpiX,…)
```

---

*Nitrogen v0.1.0 · MIT License · generated 2026-05-20*
