/**
 * @file BrowserUI.h
 * @brief Main browser chrome – coordinates all sub-systems into a single window.
 * @details Owns the tab strip, bookmarks bar, URL bar, settings panel, DevTools
 *          pane, and the active WebView canvas.  Drives the frame loop and
 *          dispatches Win32 events to the right sub-system.
 * @copyright 2026, Nitrogen Browser Project
 */

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

namespace browser { class WebView; }
namespace devtools { class DOMInspector; class PaintProfiler; }

#ifdef _WIN32
#include "core/Win32Window.h"
#endif

namespace browser {

/**
 * @struct TabEntry
 * @brief One tab strip entry (title + URL for context-menu actions).
 */
struct TabEntry {
    std::string id;        ///< Stable tab ID
    std::string title;     ///< <title> text or URL
    std::string url;       ///< Current URL
    bool        active{false};
};

/**
 * @class BrowserUI
 * @brief Top-level window chrome that wires Tab + Chrome sub-systems together.
 */
class BrowserUI {
public:
    BrowserUI();
    ~BrowserUI();

    // ── lifecycle ──────────────────────────────────────────────────────────────

    /**
     * @brief Creates the native window and starts the message / render loop.
     * @param initialUrl Homepage URL to load on first start.
     */
    void run(const std::string& initialUrl);

    /**
     * @brief Gracefully closes the window and unsubscribes event hooks.
     */
    void quit();

    // ── frame tick ─────────────────────────────────────────────────────────────

    /**
     * @brief Called once per frame by the message pump.
     *        Advances DevTools, ticks the active tab, and measures paint timing.
     * @param dtMs Delta-time since last tick.
     */
    void onFrame(double dtMs);

    // ── input ──────────────────────────────────────────────────────────────────

    /**
     * @brief Handles a VK-code key event.
     * @param vkCode  Virtual-key code (VK_RETURN, VK_TAB, VK_COMMA, …).
     * @param c       Character produced (0 for non-printable).
     * @param ctrl    True when Ctrl is held.
     * @param shift   True when Shift is held.
     * @param alt     True when Alt is held.
     */
    void onKeyDown(int vkCode, char c, bool ctrl, bool shift, bool alt);

    /**
     * @brief Handles a mouse click.
     *        Dispatches to tab strip context menu, bookmark navigation, or page.
     * @param x Viewport x-coordinate.
     * @param y Viewport y-coordinate.
     * @param button 0 = left, 1 = middle, 2 = right.
     */
    void onMouseClick(int x, int y, int button);

    /**
     * @brief Handles a mouse move (for DevTools hover bounding-box highlight).
     * @param x Viewport x-coordinate.
     * @param y Viewport y-coordinate.
     */
    void onMouseMove(int x, int y);

    // ── sub-system access ──────────────────────────────────────────────────────

    Tab*          activeTab() const;
    Bookmarks*    bookmarks()  { return bookmarks_.get(); }
    URLBar*       urlBar()     { return urlBar_.get(); }
    SettingsPanel* settings()  { return settingsPanel_.get(); }

    devtools::DOMInspector*   domInspector()   { return domInspector_.get(); }
    devtools::PaintProfiler*  paintProfiler()  { return paintProfiler_.get(); }

    /**
     * @brief Returns the URL bar text string as currently committed.
     */
    std::string currentUrl() const;

    /**
     * @brief Dispatches a navigation command (back / forward / reload).
     *        Called from the Win32 WndProc on ID_BACK, ID_FORWARD, ID_RELOAD.
     */
    void handleCommand(const std::string& cmd);

    // ── DevTools ───────────────────────────────────────────────────────────────

    /**
     * @brief Toggle  DevTools panel visibility.
     */
    void toggleDevTools();

    /**
     * @brief Whether DevTools are open.
     */
    bool devToolsOpen() const { return devToolsOpen_; }

private:
    /**
     * @brief Called by onKeyDown when Ctrl+Comma is pressed.
     */
    void openSettings();

    /**
     * @brief Called by onKeyDown when F12 is pressed.
     */
    void togglePaintProfiler();

    /**
     * @brief Tab-strip UI callback: open a new tab.
     */
    void newTab();

    /**
     * @brief Tab-strip UI callback: close the active tab.
     */
    void closeActiveTab();

    /**
     * @brief Tab-strip UI callback: activate a given tab index.
     * @param index  Tab index in tabs_.
     */
    void activateTab(size_t index);

    /**
     * @brief Renders the DevTools + profiler overlays over the page content.
     */
    void renderDevToolsOverlay();

    /**
     * @brief Builds a paint profiler overlay string from the current timing data.
     */
    std::string buildProfilerOverlayText() const;

    void saveBookmarks();

    // ── sub-systems ────────────────────────────────────────────────────────────
    std::vector<std::unique_ptr<Tab>> tabs_;   ///< All open tabs
    size_t                            activeIdx_{0};

    std::unique_ptr<Bookmarks>      bookmarks_;
    std::unique_ptr<URLBar>         urlBar_;
    std::unique_ptr<SettingsPanel>  settingsPanel_;
    std::unique_ptr<devtools::DOMInspector>  domInspector_;
    std::unique_ptr<devtools::PaintProfiler> paintProfiler_;

    bool devToolsOpen_{false};   ///< Full DevTools side-panel open
    bool profilerOpen_{false};   ///< Paint profiler overlay visible

#ifdef _WIN32
    std::unique_ptr<core::Win32Window> window_;
#endif
};

} // namespace browser

#endif // BROWSER_BROWSERUI_H
