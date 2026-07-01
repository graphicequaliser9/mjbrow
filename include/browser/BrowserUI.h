#ifndef BROWSER_BROWSERUI_H
#define BROWSER_BROWSERUI_H

#include <string>
#include <vector>
#include <memory>

#include "browser/Tab.h"
#include "browser/Bookmarks.h"
#include "browser/URLBar.h"
#include "browser/SettingsPanel.h"
#include "devtools/DOMInspector.h"
#include "devtools/PaintProfiler.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace browser { class WebView; }
namespace devtools { class DOMInspector; class PaintProfiler; }

#ifdef _WIN32
#include "core/Win32Window.h"
#endif

namespace browser {

struct TabEntry {
    std::string id;
    std::string title;
    std::string url;
    bool active{false};
};

class BrowserUI {
public:
    BrowserUI();
    ~BrowserUI();

    void run(const std::string& initialUrl);
    void run(const std::wstring& initialUrlW);
    void quit();

    void onFrame(double dtMs);
    void renderPage(HDC hdc, RECT rcClip);

    void onKeyDown(int vkCode, char c, bool ctrl, bool shift, bool alt);
    void onMouseClick(int x, int y, int button);
    void onMouseMove(int x, int y);

    Tab* activeTab() const;
    Bookmarks* bookmarks() { return bookmarks_.get(); }
    URLBar* urlBar() { return urlBar_.get(); }
    SettingsPanel* settings() { return settingsPanel_.get(); }

    devtools::DOMInspector* domInspector() { return domInspector_.get(); }
    devtools::PaintProfiler* paintProfiler() { return paintProfiler_.get(); }

    std::string currentUrl() const;

    void toggleDevTools();
    bool devToolsOpen() const { return devToolsOpen_; }

    void newTab();
    void closeActiveTab();
    void activateTab(size_t index);

private:
    void saveBookmarks();
    void openSettings();
    void togglePaintProfiler();
    void renderDevToolsOverlay();
    std::string buildProfilerOverlayText() const;

    std::vector<std::unique_ptr<Tab>> tabs_;
    size_t activeIdx_{0};

    std::unique_ptr<Bookmarks> bookmarks_;
    std::unique_ptr<URLBar> urlBar_;
    std::unique_ptr<SettingsPanel> settingsPanel_;
    std::unique_ptr<devtools::DOMInspector> domInspector_;
    std::unique_ptr<devtools::PaintProfiler> paintProfiler_;

    bool devToolsOpen_{false};
    bool profilerOpen_{false};

#ifdef _WIN32
    std::unique_ptr<core::Win32Window> window_;
#endif
};

} // namespace browser

#endif