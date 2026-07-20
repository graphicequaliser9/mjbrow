/**
 * @file tests/LayoutTest.cpp
 * @brief Unit tests for the CSS layout engine (Bead A).
 * @details Builds small DOM fragments with inline styles, runs the layout
 *          engine, and asserts the resulting LayoutNode tree has correct
 *          positions, dimensions, and hierarchy.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/Box.h"
#include "layout/LayoutNode.h"
#include "layout/TextMeasurer.h"
#include "layout/BlockLayout.h"
#include "layout/InlineLayout.h"

#include "html/DOMNode.h"
#include "html/HTMLParser.h"
#include "css/Cascade.h"
#include "css/CSSParser.h"
#include "browser/Tab.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace layout;
using namespace html;
using namespace css;
using namespace browser;

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

static const LayoutNode* findLayoutNode(const std::vector<std::unique_ptr<LayoutNode>>& tree,
                                        const std::string& tag) {
    for (const auto& node : tree) {
        if (!node) continue;
        if (node->domNode && node->domNode->tagName == tag) {
            return node.get();
        }
        const auto* found = findLayoutNode(node->children, tag);
        if (found) return found;
    }
    return nullptr;
}

static void testSimpleBlockLayout() {
    std::string html = "<html><head></head><body>"
                       "<div id='a' style='width:200px;height:50px;background:red'></div>"
                       "<p id='b' style='width:200px;height:30px;background:blue'>Hello</p>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);
    CHECK(!tree->empty());

    const LayoutNode* body = nullptr;
    for (const auto& n : *tree) {
        if (n && n->domNode && n->domNode->tagName == "body") {
            body = n.get();
            break;
        }
    }
    CHECK(body != nullptr);
    CHECK(body->width > 0.0f);
    CHECK(body->height > 0.0f);

    const LayoutNode* a = findLayoutNode(*tree, "div");
    CHECK(a != nullptr);
    CHECK(a->width == 200.0f);
    CHECK(a->height == 50.0f);
    CHECK(a->x >= 0.0f);
    CHECK(a->y >= 0.0f);

    const LayoutNode* b = findLayoutNode(*tree, "p");
    CHECK(b != nullptr);
    CHECK(b->width == 200.0f);
    CHECK(b->height == 30.0f);
    CHECK(b->y > a->y);
}

static void testInlineTextLayout() {
    std::string html = "<html><head></head><body>"
                       "<p id='para' style='width:100px'>Hello world this is a test</p>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);
    CHECK(!tree->empty());

    const LayoutNode* para = findLayoutNode(*tree, "p");
    CHECK(para != nullptr);
    CHECK(para->width > 0.0f);
    CHECK(para->height > 0.0f);
    CHECK(para->children.size() > 0);
}

static void testTextMeasurer() {
    TextMeasurer measurer;
    auto m = measurer.measure("Hello", 16.0f, "Arial");
    CHECK(m.width > 0.0f);
    CHECK(m.height > 0.0f);
    CHECK(m.ascent > 0.0f);
    CHECK(m.descent >= 0.0f);

    float lh = measurer.lineHeight(16.0f, 1.5f);
    CHECK(lh == 24.0f);

    float lh2 = measurer.lineHeight(16.0f, 0.0f);
    CHECK(lh2 > 0.0f);
}

static void testNestedBlocks() {
    std::string html = "<html><head></head><body>"
                       "<section id='outer' style='width:300px'>"
                       "<div id='inner' style='width:100px;height:40px;background:green'></div>"
                       "<p id='text' style='width:200px'>Some text here</p>"
                       "</section>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);
    CHECK(!tree->empty());

    const LayoutNode* outer = findLayoutNode(*tree, "section");
    CHECK(outer != nullptr);
    CHECK(outer->width == 300.0f);

    const LayoutNode* inner = findLayoutNode(*tree, "div");
    CHECK(inner != nullptr);
    CHECK(inner->width == 100.0f);
    CHECK(inner->height == 40.0f);
    CHECK(inner->x >= outer->x);
    CHECK(inner->y >= outer->y);
}

int main() {
    testTextMeasurer();
    testSimpleBlockLayout();
    testInlineTextLayout();
    testNestedBlocks();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
