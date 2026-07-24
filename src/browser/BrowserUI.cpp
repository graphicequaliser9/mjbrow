/**
 * @file BrowserUI.cpp
 * @brief BrowserUI implementation: frame loop, event dispatch, DevTools overlay.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/BrowserUI.h"
#include "browser/URLBar.h"
#include "browser/Bookmarks.h"
#include "browser/SettingsPanel.h"
#include "devtools/DOMInspector.h"
#include "devtools/PaintProfiler.h"
#include "core/Win32Window.h"
#include "layout/LayoutNode.h"
#include "html/DOMNode.h"
#include "css/Cascade.h"
#include "util/Logging.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
// Cross-platform fallback virtual-key constants
#ifndef VK_BACK
#define VK_BACK      0x08
#define VK_TAB       0x09
#define VK_RETURN    0x0D
#define VK_ESCAPE    0x1B
#define VK_F4        0x73
#define VK_F5        0x74
#define VK_F12       0x7B
#define VK_OEM_COMMA 0xBC
#define VK_DELETE    0x2E
#endif
#endif

namespace browser {

BrowserUI::BrowserUI()
    : bookmarks_(std::make_unique<Bookmarks>())
    , urlBar_(std::make_unique<URLBar>())
    , settingsPanel_(std::make_unique<SettingsPanel>(false))
    , domInspector_(std::make_unique<devtools::DOMInspector>())
    , paintProfiler_(std::make_unique<devtools::PaintProfiler>())
{
    urlBar_->onNavigate([](const std::string& url) {
        util::Log(util::LogLevel::Info, "BrowserUI: navigate → " + url + "\n");
    });

    settingsPanel_->onChange([](const BrowserSettings& /*s*/) {
        util::Log(util::LogLevel::Info, "BrowserUI: settings changed\n");
    });
}

BrowserUI::~BrowserUI() {
    saveBookmarks();
}

Tab* BrowserUI::activeTab() const {
    if (tabs_.empty()) return nullptr;
    return tabs_[activeIdx_].get();
}

std::string BrowserUI::currentUrl() const {
    auto* tab = activeTab();
    return tab ? tab->url() : std::string();
}

void BrowserUI::saveBookmarks() {
    try {
        bookmarks_->save();
    } catch (std::system_error& e) {
        util::Log(util::LogLevel::Error,
                  std::string("BrowserUI: failed to save bookmarks: ") + e.what() + "\n");
    }
}

void BrowserUI::run(const std::string& initialUrl) {
    // Seed the first tab BEFORE creating window (so renderPage has content on first paint)
    if (!initialUrl.empty()) {
        auto tab = std::make_unique<Tab>(initialUrl);
        tabs_.push_back(std::move(tab));
        activeIdx_ = 0;
        urlBar_->setCurrentUrl(initialUrl);
    }

#ifdef _WIN32
    window_ = std::make_unique<core::Win32Window>(this);
    if (window_) {
        util::Log(util::LogLevel::Info, "BrowserUI: window created, starting message pump\n");
    }
#else
    util::Log(util::LogLevel::Info, "BrowserUI: headless stub run()\n");
#endif

    try {
        bookmarks_->load();
    } catch (std::system_error& e) {
        util::Log(util::LogLevel::Error,
                  std::string("BrowserUI: failed to load bookmarks: ") + e.what() + "\n");
    }

    try {
        settingsPanel_->load();
    } catch (std::system_error& e) {
        util::Log(util::LogLevel::Error,
                  std::string("BrowserUI: failed to load settings: ") + e.what() + "\n");
    }

    // If the message pump is owned by Win32Window, hook DevTools overlay here
#ifdef _WIN32
    // Win32Window::run() owns the real message loop; we run a frame tick roughly
    // 60 FPS (~16 ms sleep) next to it via a WM_TIMER posted from the real loop.
    window_->run();
#else
    // Headless stub – advance one tick so the test harness can inspect state
    if (activeTab()) activeTab()->tick(16.0);
#endif
}

void BrowserUI::quit() {
    util::Log(util::LogLevel::Info, "BrowserUI: quit\n");
    saveBookmarks();
    tabs_.clear();
    activeIdx_ = 0;
#ifdef _WIN32
    window_.reset();
#endif
}

void BrowserUI::onFrame(double dtMs) {
    paintProfiler_->beginPaint();

    if (auto* tab = activeTab()) {
        tab->tick(dtMs);
        if (auto* doc = tab->document()) {
            domInspector_->attach(doc);
        }
        // Keep URL bar in sync if it hasn't been manually edited
        if (urlBar_->currentInput().empty() || !urlBar_->isDropdownOpen()) {
            urlBar_->setCurrentUrl(tab->url());
        }
    }

    paintProfiler_->beginLayout();
    paintProfiler_->endLayout();
    paintProfiler_->recordJSTime(0.0);
    paintProfiler_->endPaint();
    paintProfiler_->endFrame();
}

void BrowserUI::onKeyDown(int vkCode, char c, bool ctrl, bool shift, bool alt) {
    if (!ctrl && !shift && !alt) {
        // Forward printable chars to URL bar when it has focus
        if (urlBar_->isDropdownOpen()) {
            urlBar_->onSpecialKey(vkCode);
            return;
        }
        if (urlBar_->currentInput().empty() || vkCode == VK_BACK || vkCode == VK_DELETE) {
            urlBar_->onKeyDown(c);
            return;
        }
    }

    // URL bar consumes Ctrl+L for address focus
    if (ctrl && (c == 'l' || c == 'L')) {
        urlBar_->setFocus();
        return;
    }

    // Ctrl+, → settings panel
    if (ctrl && (vkCode == VK_OEM_COMMA || c == ',')) {
        openSettings();
        return;
    }

    // F12 → toggle paint profiler
    if (vkCode == VK_F12) {
        togglePaintProfiler();
        return;
    }

    // F5 → reload active tab
    if (vkCode == VK_F5) {
        if (auto* tab = activeTab()) {
            tab->navigate(tab->url());
        }
        return;
    }

    // Ctrl+T → new tab
    if (ctrl && c == 't') {
        newTab();
        return;
    }

    // Ctrl+W / Ctrl+F4 → close active tab
    if ((ctrl && c == 'w') || (ctrl && vkCode == VK_F4)) {
        closeActiveTab();
        return;
    }
}

void BrowserUI::onMouseClick(int x, int y, int button) {
    if (button == 2) {
        // Right-click on tab strip → context menu (copy / duplicate URL)
        auto* tab = activeTab();
        if (tab) {
            util::Log(util::LogLevel::Info, "BrowserUI: tab context menu for " + tab->url() + "\n");
        }
        return;
    }

    if (activeTab()) {
        // Left-click: delegate to page
        (void)x; (void)y; (void)button;
    }
}

void BrowserUI::onMouseMove(int x, int y) {
    if (domInspector_->isVisible() && domInspector_->getRoot()) {
        // Detect which tree node covers (x, y) and update hover overlay
        auto tree = domInspector_->flattenTree();
        std::vector<size_t> candidateIndices;
        for (size_t i = 0; i < tree.size(); ++i) {
            auto* n = tree[i];
            if (n && n->domNode) {
                candidateIndices.push_back(i);
            }
        }
        auto ov = domInspector_->getHoverOverlay(nullptr);

        (void)x; (void)y; (void)candidateIndices; (void)ov;
    }
}

void BrowserUI::openSettings() {
    settingsPanel_->open();
    util::Log(util::LogLevel::Info, "BrowserUI: settings panel opened\n");
}

void BrowserUI::togglePaintProfiler() {
    profilerOpen_ = !profilerOpen_;
    paintProfiler_->setVisible(profilerOpen_);
    util::Log(util::LogLevel::Info,
              "BrowserUI: paint profiler " + std::string(profilerOpen_ ? "on" : "off") + "\n");
}

void BrowserUI::newTab() {
    auto tab = std::make_unique<Tab>();
    tabs_.push_back(std::move(tab));
    activeIdx_ = tabs_.size() - 1;
    urlBar_->setCurrentUrl("about:blank");
    util::Log(util::LogLevel::Info, "BrowserUI: new tab #" + std::to_string(tabs_.size()) + "\n");
}

void BrowserUI::closeActiveTab() {
    if (tabs_.size() <= 1) return;
    tabs_.erase(tabs_.begin() + activeIdx_);
    activeIdx_ = std::min<size_t>(activeIdx_, tabs_.size() - 1);
    if (auto* tab = activeTab()) {
        urlBar_->setCurrentUrl(tab->url());
    }
    util::Log(util::LogLevel::Info, "BrowserUI: closed tab, " + std::to_string(tabs_.size()) + " remain\n");
}

void BrowserUI::activateTab(size_t index) {
    if (index < tabs_.size()) {
        activeIdx_ = index;
        if (auto* tab = activeTab()) {
            urlBar_->setCurrentUrl(tab->url());
        }
    }
}

std::string BrowserUI::buildProfilerOverlayText() const {
    if (!profilerOpen_ || !paintProfiler_) return "";
    auto timing  = paintProfiler_->getLastFrameTiming();
    std::ostringstream ss;
    ss << "FPS: " << std::fixed << std::setprecision(0) << timing.fps << " | "
       << "paint: " << std::fixed << std::setprecision(1)
       << (timing.paintUs  / 1000.0) << " ms | "
       << "layout: "           << (timing.layoutUs / 1000.0) << " ms | "
       << "JS: "               << (timing.jsUs     / 1000.0) << " ms";
    return ss.str();
}

void BrowserUI::toggleDevTools() {
    devToolsOpen_ = !devToolsOpen_;
    domInspector_->setVisible(devToolsOpen_);
    util::Log(util::LogLevel::Info,
              "BrowserUI: DevTools " + std::string(devToolsOpen_ ? "open" : "closed") + "\n");
}

void BrowserUI::renderDevToolsOverlay() {
    if (!devToolsOpen_ || !domInspector_->isVisible() || !domInspector_->getRoot())
        return;

    auto overlayLine = buildProfilerOverlayText();
    if (!overlayLine.empty()) {
        // Stub: in the full engine this is drawn via Painter::drawText in the
        // top-right corner of the viewport after the page paint completes.
        util::Log(util::LogLevel::Trace,
                  "DevTools overlay: " + overlayLine + "\n");
    }
}

void BrowserUI::renderPage(HDC hdc, RECT rcClip) {
    if (auto* tab = activeTab()) {
        HBRUSH hbrWhite = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcClip, hbrWhite);
        DeleteObject(hbrWhite);

        html::DOMNode* doc = tab->document();
        if (!doc) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(128, 0, 0));
            DrawTextA(hdc, "No DOM document (doc=null)", -1, &rcClip, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        const layout::LayoutNode* root = tab->layoutRoot();
        if (!root) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(128, 0, 0));
            DrawTextA(hdc, "No layout tree (layoutRoot=null)", -1, &rcClip, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));

        int scrollOffset = tab->scrollOffsetY();
        int viewportHeight = rcClip.bottom - rcClip.top;
        int maxScroll = std::max(0, static_cast<int>(root->height) - viewportHeight);
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
        if (scrollOffset < 0) scrollOffset = 0;

        std::function<void(const layout::LayoutNode*)> renderNode =
            [&](const layout::LayoutNode* node) {
                if (!node || !node->domNode) return;

                auto* domNode = node->domNode;

                if (domNode->nodeType == html::NodeType::Element) {
                    if (domNode->tagName == "title") {
                        std::string title;
                        for (html::DOMNode* c = domNode->firstChild; c; c = c->nextSibling) {
                            if (c->nodeType == html::NodeType::Text) {
                                title += c->textContent;
                            }
                        }
                        if (window_) {
                            int size_needed = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), (int)title.size(), nullptr, 0);
                            std::wstring titleW(size_needed, 0);
                            MultiByteToWideChar(CP_UTF8, 0, title.c_str(), (int)title.size(), &titleW[0], size_needed);
                            SetWindowTextW(window_->hwnd(), titleW.c_str());
                        }
                        return;
                    }
                    if (domNode->tagName == "style" || domNode->tagName == "script") return;

                    css::ComputedStyle style = css::Cascade::computeStyle(domNode, doc);
                    if (style.display == css::ComputedStyle::None) return;

                    bool isBlock = node->isBlock;

                    if (isBlock) {
                        int nodeBottom = node->y + node->height;
                        if (nodeBottom <= scrollOffset || node->y >= scrollOffset + viewportHeight) {
                            return;
                        }

                        int left = node->x;
                        int top = node->y - scrollOffset;
                        int right = node->x + node->width;
                        int bottom = node->y + node->height - scrollOffset;

                        RECT boxRc{left, top, right, bottom};
                        if (style.backgroundColor != 0x00000000) {
                            HBRUSH hbrBg = CreateSolidBrush(RGB(
                                (style.backgroundColor >> 16) & 0xFF,
                                (style.backgroundColor >> 8) & 0xFF,
                                style.backgroundColor & 0xFF
                            ));
                            FillRect(hdc, &boxRc, hbrBg);
                            DeleteObject(hbrBg);
                        }
                        if (style.borderTop > 0 || style.borderRight > 0 ||
                            style.borderBottom > 0 || style.borderLeft > 0) {
                            HBRUSH hbrBorder = CreateSolidBrush(RGB(
                                (style.borderColor >> 16) & 0xFF,
                                (style.borderColor >> 8) & 0xFF,
                                style.borderColor & 0xFF
                            ));
                            if (style.borderTop > 0) {
                                RECT topRc{boxRc.left, boxRc.top, boxRc.right, boxRc.top + static_cast<int>(style.borderTop)};
                                FillRect(hdc, &topRc, hbrBorder);
                            }
                            if (style.borderRight > 0) {
                                RECT rightRc{boxRc.right - static_cast<int>(style.borderRight), boxRc.top, boxRc.right, boxRc.bottom};
                                FillRect(hdc, &rightRc, hbrBorder);
                            }
                            if (style.borderBottom > 0) {
                                RECT bottomRc{boxRc.left, boxRc.bottom - static_cast<int>(style.borderBottom), boxRc.right, boxRc.bottom};
                                FillRect(hdc, &bottomRc, hbrBorder);
                            }
                            if (style.borderLeft > 0) {
                                RECT leftRc{boxRc.left, boxRc.top, boxRc.left + static_cast<int>(style.borderLeft), boxRc.bottom};
                                FillRect(hdc, &leftRc, hbrBorder);
                            }
                            DeleteObject(hbrBorder);
                        } else if (style.backgroundColor == 0x00000000) {
                            HBRUSH hbrBox = CreateSolidBrush(RGB(150, 175, 200));
                            FrameRect(hdc, &boxRc, hbrBox);
                            DeleteObject(hbrBox);
                        }

                        int px = static_cast<int>(style.paddingLeft);
                        int py = static_cast<int>(style.paddingTop);
                        int pw = static_cast<int>(style.paddingRight);
                        int pb = static_cast<int>(style.paddingBottom);
                        int cx = left + px;
                        int cy = top + py;
                        int cright = right - pw;
                        int cbottom = bottom - pb;
                        int cw = cright - cx;
                        int ch = cbottom - cy;

                        if (cw > 0 && ch > 0) {
                            int textY = cy;
                            std::function<void(html::DOMNode*, const css::ComputedStyle&)> drawTextNode =
                                [&](html::DOMNode* dn, const css::ComputedStyle& parentStyle) {
                                    if (!dn) return;
                                    if (dn->nodeType == html::NodeType::Text) {
                                        std::string text = dn->textContent;
                                        bool onlyWs = true;
                                        for (char c : text) if (!std::isspace(static_cast<unsigned char>(c))) { onlyWs = false; break; }
                                        if (onlyWs) return;

                                        const css::ComputedStyle* style = &parentStyle;
                                        int weight = style->fontWeight;
                                        if (weight < 400) weight = 400;
                                        if (weight > 900) weight = 900;
                                        HFONT hFont = CreateFontA(
                                            -static_cast<int>(style->fontSize),
                                            0, 0, 0, weight,
                                            FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET,
                                            OUT_DEFAULT_PRECIS,
                                            CLIP_DEFAULT_PRECIS,
                                            DEFAULT_QUALITY,
                                            DEFAULT_PITCH | FF_DONTCARE,
                                            style->fontFamily.c_str()
                                        );
                                        HFONT hOld = static_cast<HFONT>(SelectObject(hdc, hFont));
                                        SetTextColor(hdc, RGB(
                                            (style->color >> 16) & 0xFF,
                                            (style->color >> 8) & 0xFF,
                                            style->color & 0xFF
                                        ));
                                        RECT rc{cx, textY, cright, cbottom};
                                        UINT flags = DT_TOP | DT_WORDBREAK | DT_CALCRECT;
                                        if (style->textAlign == css::ComputedStyle::AlignCenter) flags |= DT_CENTER;
                                        else if (style->textAlign == css::ComputedStyle::AlignRight) flags |= DT_RIGHT;
                                        else flags |= DT_LEFT;
                                        DrawTextA(hdc, text.c_str(), -1, &rc, flags);
                                        DrawTextA(hdc, text.c_str(), -1, &rc, flags & ~DT_CALCRECT);
                                        SelectObject(hdc, hOld);
                                        DeleteObject(hFont);
                                        textY = rc.bottom + 2;
                                    } else if (dn->nodeType == html::NodeType::Element) {
                                        if (dn->tagName == "style" || dn->tagName == "script" || dn->tagName == "title") return;
                                        css::ComputedStyle childStyle = css::Cascade::computeStyle(dn, doc);
                                        if (childStyle.display == css::ComputedStyle::None) return;
                                        if (childStyle.display == css::ComputedStyle::Block) return;
                                        for (html::DOMNode* c = dn->firstChild; c; c = c->nextSibling) {
                                            drawTextNode(c, childStyle);
                                        }
                                    }
                                };

                            for (html::DOMNode* c = domNode->firstChild; c; c = c->nextSibling) {
                                drawTextNode(c, style);
                            }
                        }

                        for (const layout::LayoutNode* child = node->firstChild; child; child = child->nextSibling) {
                            renderNode(child);
                        }
                    } else {
                        for (const layout::LayoutNode* child = node->firstChild; child; child = child->nextSibling) {
                            renderNode(child);
                        }
                    }
                }
            };

        renderNode(root);
    }
}

// Wide-string overload for VS template integration
void BrowserUI::run(const std::wstring& initialUrlW) {
    // Convert wide string to UTF-8 string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &initialUrlW[0], (int)initialUrlW.size(), nullptr, 0, nullptr, nullptr);
    std::string initialUrl(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &initialUrlW[0], (int)initialUrlW.size(), &initialUrl[0], size_needed, nullptr, nullptr);
    run(initialUrl);  // Call the std::string version
}

} // namespace browser
