/**
 * @file Tab.h
 * @brief Single browser tab: owns one DOM document, one JS VM, and URL state.
 * @details Each Tab is an independent rendering context.  Multiple tabs can
 *          coexist in the same BrowserUI; only the active tab receives frame
 *          ticks and input events.  The owning TabStrip manages the tab bar
 *          chrome.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef BROWSER_TAB_H
#define BROWSER_TAB_H

#include <string>
#include <memory>

// Forward declarations – we include the real headers only in Tab.cpp.
namespace html { class DOMNode; }
namespace js   { class VM; }
namespace css  { class CSSParser; class ComputedStyle; }

namespace browser {

/**
 * @struct WebView
 * @brief Opaque render state for one tab (canvas + viewport scroll).
 * @details In a full implementation this holds a bitmap surface, scroll
 *          offset, clip rect, and a reference back to its owning Tab.
 */
struct WebView {
    int scrollX{0};   ///< Horizontal scroll offset in CSS pixels
    int scrollY{0};   ///< Vertical   scroll offset in CSS pixels
    int width{800};   ///< Viewport width  in CSS pixels
    int height{600};  ///< Viewport height in CSS pixels
};

/**
 * @class Tab
 * @brief One browser tab with its own document, JS engine, and URL state.
 */
class Tab {
public:
    Tab();
    explicit Tab(const std::string& initialUrl);
    ~Tab();

    // ── lifecycle ───────────────────────────────────────────────────────────────

    /**
     * @brief Navigates this tab to the given URL.
     *        Fetches HTML, reparses, rebuilds layout, and resets the viewport.
     * @param url The target URL.
     */
    void navigate(const std::string& url);

    // ── frame tick ─────────────────────────────────────────────────────────────

    /**
     * @brief Advances the tab by one frame: layout, paint, then tick the JS VM.
     * @param dtMs Delta-time since last frame in milliseconds (used for JS RAF).
     */
    void tick(double dtMs);

    // ── navigation commands ─────────────────────────────────────────────────────

    /**
     * @brief Navigate back in history (stub implementation).
     */
    void goBack();

    /**
     * @brief Navigate forward in history (stub implementation).
     */
    void goForward();

    /**
     * @brief Reload the current page (stub implementation).
     */
    void goReload();

    // ── state access ───────────────────────────────────────────────────────────

    /**
     * @brief Returns the current URL of this tab.
     */
    std::string url() const { return url_; }

    /**
     * @brief Returns the active WebView.
     */
    WebView* webView() { return &webView_; }
    const WebView* webView() const { return &webView_; }

    /**
     * @brief Returns the root DOM node (or nullptr if no document loaded).
     */
    html::DOMNode* document() const { return document_.get(); }

    /**
     * @brief Returns the title for the tab strip (<title> element textContent).
     */
    std::string title() const;

    /**
     * @brief Whether the tab has finished loading.
     */
    bool isLoading() const { return loading_; }

    /**
     * @brief Frees the live document and resets the tab to a blank state.
     */
    void clear();

    // ── DevTools helpers ───────────────────────────────────────────────────────

    /**
     * @brief Returns the parsed but unscoped VM so DevTools can inspect it.
     */
    js::VM* vm() const;

private:
    /**
     * @brief Parses the raw HTML string into a DOM tree + populates document_.
     */
    void parseHTML(const std::string& html);

    /**
     * @brief Runs the CSS cascade over document_ to fill ComputedStyle pointers.
     */
    void cascadeStyles();

    /**
     * @brief Runs the layout engine and populates layout boxes.
     */
    void performLayout();

    /**
     * @brief Paints the current frame into the WebView's surface.
     */
    void paintFrame();

    std::string url_;                      ///< Navigated URL
    std::string rawHtml_;                  ///< Last-fetched raw source
    std::unique_ptr<html::DOMNode> document_; ///< Live DOM tree
    std::unique_ptr<js::VM>     vm_;       ///< Per-tab JS engine
    WebView  webView_;                     ///< Viewport + scroll state
    bool     loading_{true};               ///< True until first paint completes
    double   paintUs_{0.0};                ///< Last paint duration (us)
    double   layoutUs_{0.0};               ///< Last layout duration (us)
    double   jsUs_{0.0};                   ///< Last JS VM duration (us)
};

} // namespace browser

#endif // BROWSER_TAB_H
