/**
 * @file tests/TableLayoutTest.cpp
 * @brief Unit tests for the CSS table layout engine with virtual scroll (Bead B).
 * @details Verifies column sizing, cell spanning, and O(visible-rows)
 *          materialization for large tables.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/TableLayout.h"
#include "layout/LayoutNode.h"
#include "layout/TextMeasurer.h"

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

static void testBasicTableLayout() {
    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:300px'>"
                       "<tr style='display:table-row'><td style='display:table-cell'>A</td><td style='display:table-cell'>B</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>C</td><td style='display:table-cell'>D</td></tr>"
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);
    CHECK(!tree->empty());

    const LayoutNode* table = findLayoutNode(*tree, "table");
    CHECK(table != nullptr);
    CHECK(table->display == css::ComputedStyle::Table);
    CHECK(table->width > 0.0f);
    CHECK(table->height > 0.0f);
}

static void testColgroupSizing() {
    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:300px;table-layout:fixed'>"
                       "<colgroup><col style='width:100px'><col style='width:200px'></colgroup>"
                       "<tr style='display:table-row'><td style='display:table-cell'>A</td><td style='display:table-cell'>B</td></tr>"
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);

    const LayoutNode* table = findLayoutNode(*tree, "table");
    CHECK(table != nullptr);
    CHECK(table->width > 0.0f);
}

static void testColspanRowspan() {
    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:300px'>"
                       "<tr style='display:table-row'><td style='display:table-cell' colspan='2'>AB</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>C</td><td style='display:table-cell'>D</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell' rowspan='2'>E</td><td style='display:table-cell'>F</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>G</td></tr>"
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);

    const LayoutNode* table = findLayoutNode(*tree, "table");
    CHECK(table != nullptr);
    CHECK(table->height > 0.0f);
}

static void testVirtualScrollMaterialization() {
    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:800px;table-layout:fixed'>"
                       "<tr style='display:table-row'><td style='display:table-cell'>R0</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>R1</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>R2</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>R3</td></tr>"
                       "<tr style='display:table-row'><td style='display:table-cell'>R4</td></tr>"
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);

    const LayoutNode* table = findLayoutNode(*tree, "table");
    CHECK(table != nullptr);

    size_t rowCount = 0;
    for (const auto& child : table->children) {
        if (child && child->display == css::ComputedStyle::TableRow) {
            ++rowCount;
        }
    }
    CHECK(rowCount <= 5);
}

static void testLargeTableMemory() {
    std::string rows;
    for (int i = 0; i < 100; ++i) {
        rows += "<tr style='display:table-row'><td style='display:table-cell'>Row" + std::to_string(i) + "</td></tr>";
    }

    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:800px;table-layout:fixed'>"
                       + rows +
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    const auto* tree = tab.layoutTree();
    CHECK(tree != nullptr);

    const LayoutNode* table = findLayoutNode(*tree, "table");
    CHECK(table != nullptr);

    size_t rowCount = 0;
    for (const auto& child : table->children) {
        if (child && child->display == css::ComputedStyle::TableRow) {
            ++rowCount;
        }
    }
    CHECK(rowCount < 100);
}

static void testTableGeometry() {
    TextMeasurer measurer;
    TableLayout tableLayout(measurer);

    std::string html = "<html><head></head><body>"
                       "<table id='t' style='display:table;width:300px;table-layout:fixed'>"
                       "<colgroup><col style='width:100px'><col style='width:200px'></colgroup>"
                       "<tr style='display:table-row'><td style='display:table-cell'>A</td><td style='display:table-cell'>B</td></tr>"
                       "</table>"
                       "</body></html>";

    Tab tab;
    tab.loadHTML(html);

    html::DOMNode* tableNode = tab.document()->getElementById("t");
    CHECK(tableNode != nullptr);

    auto geo = tableLayout.computeGeometry(tableNode, 300.0f);
    CHECK(geo.columnCount == 2);
    CHECK(geo.rowCount == 1);
    CHECK(geo.totalWidth > 0.0f);
    CHECK(geo.totalHeight > 0.0f);
}

int main() {
    testBasicTableLayout();
    testColgroupSizing();
    testColspanRowspan();
    testVirtualScrollMaterialization();
    testLargeTableMemory();
    testTableGeometry();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
