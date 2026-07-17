/**
 * @file tests/StyleRenderTest.cpp
 * @brief Unit tests for style rendering (Bead C).
 * @details Verifies that the cascade engine produces ComputedStyle structs
 *          with the correct values for inline styles and stylesheet rules,
 *          and that Tab::cascadeStyles() correctly populates document stylesheets.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/Cascade.h"
#include "css/CSSParser.h"
#include "html/DOMNode.h"
#include "browser/Tab.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace css;
using namespace html;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "FAIL: " #cond "  (" << __FILE__ << ":" << __LINE__ \
                      << ")\n";                                             \
        }                                                                    \
    } while (0)

static DOMNode* makeElement(const std::string& tag, const std::string& id = "",
                            const std::string& cls = "") {
    DOMNode* n = new DOMNode();
    n->nodeType = NodeType::Element;
    n->tagName = tag;
    if (!id.empty()) n->setAttribute("id", id);
    if (!cls.empty()) n->setAttribute("class", cls);
    return n;
}

static void testInlineStyleComputedStyle() {
    DOMNode* div = makeElement("div", "box");
    div->setAttribute("style", "color: red; font-size: 24px; text-align: center");

    Document doc;
    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.color == 0xFFFF0000);
    CHECK(style.fontSize == 24.0f);
    CHECK(style.textAlign == css::ComputedStyle::AlignCenter);
    CHECK(style.fontFamily == "Arial");
    CHECK(style.fontWeight == 400);

    delete div;
}

static void testStylesheetComputedStyle() {
    DOMNode* div = makeElement("div", "box");

    Document doc;
    CSSParser parser;
    doc.stylesheets = parser.parse("div { color: blue; padding: 10px; }");

    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.color == 0xFF0000FF);
    CHECK(style.paddingTop == 10.0f);
    CHECK(style.paddingRight == 10.0f);
    CHECK(style.paddingBottom == 10.0f);
    CHECK(style.paddingLeft == 10.0f);

    delete div;
}

static void testInlineOverridesStylesheet() {
    DOMNode* div = makeElement("div", "box");
    div->setAttribute("style", "color: green; background-color: yellow");

    Document doc;
    CSSParser parser;
    doc.stylesheets = parser.parse("div { color: red; }");

    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.color == 0xFF00FF00);
    CHECK(style.backgroundColor == 0xFFFFFF00);

    delete div;
}

static void testMarginAndBorder() {
    DOMNode* div = makeElement("div");
    div->setAttribute("style", "margin-top: 5px; margin-left: 10px; border-top-width: 2px; border-color: purple");

    Document doc;
    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.marginTop == 5.0f);
    CHECK(style.marginLeft == 10.0f);
    CHECK(style.borderTop == 2.0f);
    CHECK(style.borderColor == 0xFF800080);

    delete div;
}

static void testTextAlignDefaults() {
    DOMNode* div = makeElement("div");

    Document doc;
    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.textAlign == css::ComputedStyle::AlignLeft);
    CHECK(style.fontSize == 16.0f);
    CHECK(style.color == 0xFF000000);

    delete div;
}

static void testDisplayAndPosition() {
    DOMNode* div = makeElement("div");
    div->setAttribute("style", "display: none; position: absolute; width: 100px; height: 50px");

    Document doc;
    css::ComputedStyle style = Cascade::computeStyle(div, &doc);
    CHECK(style.display == css::ComputedStyle::None);
    CHECK(style.position == css::ComputedStyle::Absolute);
    CHECK(style.width == 100.0f);
    CHECK(style.height == 50.0f);

    delete div;
}

static void testTabCascadeCollectsStylesheets() {
    browser::Tab tab;
    tab.loadHTML(
        "<html><head>"
        "<style>div { color: navy; }</style>"
        "</head><body>"
        "<div id='x'>Hello</div>"
        "</body></html>");

    html::Document* doc = static_cast<html::Document*>(tab.document());
    CHECK(doc != nullptr);
    CHECK(!doc->stylesheets.empty());

    html::DOMNode* div = doc->querySelector("#x");
    CHECK(div != nullptr);

    css::ComputedStyle style = Cascade::computeStyle(div, doc);
    CHECK(style.color == 0xFF000080);
}

static void testAcceptanceCase() {
    browser::Tab tab;
    tab.loadHTML(
        "<html><body>"
        "<div style='color: red; font-size: 24px; text-align: center;'>Hello</div>"
        "</body></html>");

    html::Document* doc = static_cast<html::Document*>(tab.document());
    CHECK(doc != nullptr);

    html::DOMNode* div = nullptr;
    for (html::DOMNode* c = doc->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Element && c->tagName == "html") {
            for (html::DOMNode* b = c->firstChild; b; b = b->nextSibling) {
                if (b->nodeType == html::NodeType::Element && b->tagName == "body") {
                    for (html::DOMNode* e = b->firstChild; e; e = e->nextSibling) {
                        if (e->nodeType == html::NodeType::Element && e->tagName == "div") {
                            div = e;
                            break;
                        }
                    }
                }
            }
        }
    }
    CHECK(div != nullptr);

    css::ComputedStyle style = Cascade::computeStyle(div, doc);
    CHECK(style.color == 0xFFFF0000);
    CHECK(style.fontSize == 24.0f);
    CHECK(style.textAlign == css::ComputedStyle::AlignCenter);
}

int main() {
    testInlineStyleComputedStyle();
    testStylesheetComputedStyle();
    testInlineOverridesStylesheet();
    testMarginAndBorder();
    testTextAlignDefaults();
    testDisplayAndPosition();
    testTabCascadeCollectsStylesheets();
    testAcceptanceCase();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
