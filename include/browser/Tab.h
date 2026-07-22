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
#include <vector>

#include "html/DOMNode.h"

// Forward declarations – we include the real headers only in Tab.cpp.
namespace html { class DOMNode; }
namespace js   { class VM; }
namespace quickjs { struct JSRuntime; struct JSContext; struct JSObject; }
namespace css  { class CSSParser; class ComputedStyle; }
namespace layout {
class LayoutNode;
class Box;
}

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

    /**
     * @brief Navigation shortcuts for toolbar/menu.
     */
void goBack()    { /* TODO: history */ navigate(url_); }
    void goForward() { /* TODO: forward history */ }
    void goReload()  { if (!url_.empty()) navigate(url_); }

    // ── frame tick ─────────────────────────────────────────────────────────────

    /**
     * @brief Advances the tab by one frame: layout, paint, then tick the JS VM.
     * @param dtMs Delta-time since last frame in milliseconds (used for JS RAF).
     */
    void tick(double dtMs);

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
     * @brief Returns the root of the layout tree (or nullptr before first layout).
     */
    const layout::LayoutNode* layoutRoot() const { return layoutRoot_; }

    std::string bodyText() const { return bodyText_; }

    /**
     * @brief Returns the concatenation of every Text node in the document
     *        (not just the body), built by walking the full DOM tree.
     */
    std::string allText() const;

    // ── DOM query API (Bead 4) ───────────────────────────────────────────────

    /**
     * @brief Returns the first element in the document with the given id.
     */
    html::DOMNode* getElementById(const std::string& id);

    /**
     * @brief Returns every element whose tagName matches @p tag (case-insensitive).
     */
    std::vector<html::DOMNode*> getElementsByTagName(const std::string& tag);

    /**
     * @brief Returns the first element matching @p selector.
     */
    html::DOMNode* querySelector(const std::string& selector);

    /**
     * @brief Returns all elements matching @p selector, in document order.
     */
    std::vector<html::DOMNode*> querySelectorAll(const std::string& selector);

    /**
     * @brief Parses an HTML string directly into this tab (no network fetch).
     *        Used by the integration pipeline and headless harnesses.
     */
    void loadHTML(const std::string& html);

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
    std::string bodyText_;                   ///< Extracted body text for rendering
    std::unique_ptr<html::Document> document_; ///< Owned parsed document
    std::unique_ptr<js::VM> vm_;            ///< Per-tab JS engine (legacy stub retained)
    WebView  webView_;                     ///< Viewport + scroll state
    layout::LayoutNode* layoutRoot_{nullptr};  ///< Root of the positioned layout tree
    bool     loading_{true};               ///< True until first paint completes
    double   paintUs_{0.0};                ///< Last paint duration (us)
    double   layoutUs_{0.0};               ///< Last layout duration (us)
    double   jsUs_{0.0};                    ///< Last JS VM duration (us)

    // ── QuickJS embedding (Bead B) ───────────────────────────────────────────
    quickjs::JSRuntime* rt_{nullptr};      ///< Owned JS runtime
    quickjs::JSContext* js_{nullptr};      ///< Per-tab JS context

    /**
     * @brief Initialises the QuickJS runtime + context for this tab and injects
     *        the browser JS globals (document, requestAnimationFrame, timers, etc.).
     */
    void initJS();

    /**
     * @brief Runs every <script> element found in the loaded DOM.
     */
    void runScripts();

    /**
     * @brief Executes a single JS source string, logging any exception.
     */
    void evalScript(const std::string& code);

    /**
     * @brief Builds the JS `document` object backed by this tab's DOM tree.
     */
    void bindDOM();

    /**
     * @brief Wraps a native DOMNode as a JS object with the DOM accessor surface.
     */
    static quickjs::JSObject* wrapNode(html::DOMNode* node);

    /**
     * @brief Advances the JS runtime by one frame: runs microtasks/pending jobs
     *        and fires requestAnimationFrame callbacks.
     */
    void runJSEventLoop();
};

} // namespace browser

#endif // BROWSER_TAB_H
