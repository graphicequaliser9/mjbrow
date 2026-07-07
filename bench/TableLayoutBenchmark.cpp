/**
 * @file TableLayoutBenchmark.cpp
 * @brief Memory + correctness benchmark for the virtual-scroll table engine.
 * @details Verifies the two hard requirements from Bead 5:
 *   - A 3500 x 250 table lays out in < 5 MB of layout memory.
 *   - LayoutBox count stays O(visibleRows x columns), never O(all cells).
 *   - colSpan / rowSpan / <colgroup> widths are honoured.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/Box.h"
#include "html/DOMNode.h"
#include "html/HTMLParser.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  ok   - %s\n", msg);
    } else {
        std::printf("  FAIL - %s\n", msg);
        ++g_failures;
    }
}

html::DOMNode* findFirstTable(html::DOMNode* node) {
    if (!node) return nullptr;
    if (node->nodeType == html::NodeType::Element &&
        node->tagName == "table") {
        return node;
    }
    for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
        html::DOMNode* found = findFirstTable(c);
        if (found) return found;
    }
    return nullptr;
}

// ── Requirement 5: 3500 x 250 table in < 5 MB ────────────────────────────────
void benchmarkLargeTable() {
    std::printf("[benchmark] 3500 x 250 virtual table\n");

    layout::TableLayoutEngine engine;
    engine.prepareFromSpec({/*rows=*/3500, /*cols=*/250, /*colWidth=*/100.0f, /*rowHeight=*/22.0f});

    check(engine.rowCount() == 3500, "row count is 3500");
    check(engine.columnCount() == 250, "column count is 250");
    check(engine.totalHeight() == 3500.0f * 22.0f, "total height = rows * rowHeight");

    // A typical viewport: 800 x 600 scrolled to the middle of the table.
    const double kViewportH = 600.0;
    const double kScrollY = 1234.0;  // arbitrary scroll position
    engine.layout(kScrollY, kViewportH);

    const size_t memBytes = engine.layoutMemoryBytes();
    const size_t mb = memBytes / (1024u * 1024u);
    std::printf("  info  - layout memory: %zu bytes (%.2f MB), boxes: %zu, cap: %zu\n",
                memBytes, static_cast<double>(memBytes) / (1024.0 * 1024.0),
                engine.boxes().size(), engine.maxBoxesForLastLayout());

    check(memBytes < 5u * 1024u * 1024u, "layout memory < 5 MB");
    check(mb < 5, "layout memory under 5 MB (integer MB check)");

    // O(visibleRows x columns) bound: a full materialisation would be 875000 boxes.
    const size_t fullCells = 3500u * 250u;
    std::printf("  info  - boxes: %zu vs full-cell count: %zu\n", engine.boxes().size(), fullCells);
    check(engine.boxes().size() <= engine.maxBoxesForLastLayout(), "box count within capacity");
    check(engine.maxBoxesForLastLayout() < 50000, "capacity bounded by visible window (~40 rows x 250)");
    check(engine.boxes().size() < fullCells / 10, "boxes are a tiny fraction of all cells");

    // Viewport culling: every visible box must intersect the viewport band.
    bool allInside = true;
    for (const layout::LayoutBox& b : engine.boxes()) {
        const float bottom = b.y + b.height;
        if (b.y > (kScrollY + kViewportH) || bottom < kScrollY) { allInside = false; break; }
    }
    check(allInside, "all visible boxes intersect the viewport band");

    // Virtual scroll to the very bottom: boxes must shift to the last rows and
    // the memory cap must still hold (no full-table materialisation).
    const double bottomScroll = engine.totalHeight() - kViewportH;
    engine.layout(bottomScroll, kViewportH);
    int minRow = engine.rowCount(), maxRow = -1;
    for (const layout::LayoutBox& b : engine.boxes()) {
        if (b.row < minRow) minRow = b.row;
        if (b.row > maxRow) maxRow = b.row;
    }
    check(engine.boxes().size() <= engine.maxBoxesForLastLayout(),
          "bottom scroll: boxes within capacity");
    check(engine.layoutMemoryBytes() < 5u * 1024u * 1024u, "bottom scroll: memory < 5 MB");
    check(minRow >= engine.rowCount() - 40, "bottom scroll: boxes are near the last rows");
    check(maxRow < engine.rowCount(), "bottom scroll: no box beyond the final row");
}

// ── Requirement 1: colSpan / rowSpan / <colgroup> ─────────────────────────────
void testColgroupAndSpans() {
    std::printf("[test] colgroup + colspan + rowspan\n");

    const std::string html =
        "<table>"
        "  <colgroup><col width=\"50\"><col width=\"100\"><col width=\"75\"></colgroup>"
        "  <tr><td>r0c0</td><td colspan=\"2\">r0span</td></tr>"
        "  <tr><td rowspan=\"2\">left</td><td>r1b</td><td>r1c</td></tr>"
        "  <tr><td>r2b</td><td>r2c</td></tr>"
        "</table>";

    html::HTMLParser parser;
    html::DOMNode* doc = parser.parse(html);
    html::DOMNode* table = findFirstTable(doc);
    check(table != nullptr, "found <table> in parsed DOM");

    layout::TableLayoutEngine engine;
    bool ok = engine.prepare(table);
    check(ok, "engine prepared");
    check(engine.columnCount() == 3, "3 columns from <colgroup>");
    check(engine.rowCount() == 3, "3 rows");

    // Column x-offsets from the explicit widths.
    check(engine.totalWidth() == 225.0f, "total width = 50+100+75");

    // Layout the whole table (tall viewport) and inspect the boxes.
    engine.layout(0.0, 1000.0);

    // Expected materialised boxes (see header docs for the worked example):
    //  row0: [r0c0], [r0span]        -> 2
    //  row1: [left], [r1b], [r1c]    -> 3
    //  row2: [r2b], [r2c]            -> 2   (left is a rowspan already emitted)
    check(engine.boxes().size() == 7, "7 boxes materialised for 3x3 with spans");

    int headers = 0, rowSpans = 0, colSpans = 0;
    for (const layout::LayoutBox& b : engine.boxes()) {
        if (b.isHeader) ++headers;
        if (b.rowSpan > 1) ++rowSpans;
        if (b.colSpan > 1) ++colSpans;
    }
    check(headers == 0, "no <th> cells (all <td>)");
    check(rowSpans == 1, "exactly one rowspan cell (left)");
    check(colSpans == 1, "exactly one colspan cell (r0span)");

    // The colspan cell must span columns 1..2 => width 175.
    bool foundColSpan = false;
    for (const layout::LayoutBox& b : engine.boxes()) {
        if (b.colSpan == 2) {
            foundColSpan = (b.width == 175.0f) && (b.x == 50.0f);
        }
    }
    check(foundColSpan, "colspan cell covers cols 1-2 (x=50, w=175)");

    // The rowspan cell lives at row 1, covers rows 1-2 => height = 2*rowHeight.
    bool foundRowSpan = false;
    for (const layout::LayoutBox& b : engine.boxes()) {
        if (b.rowSpan == 2) {
            foundRowSpan = (b.row == 1) && (b.height == 2.0f * engine.rowHeight());
        }
    }
    check(foundRowSpan, "rowspan cell anchored at row 1 with height 2*rowHeight");
}

} // namespace

int main() {
    std::printf("=== TableLayoutEngine benchmark ===\n");
    benchmarkLargeTable();
    testColgroupAndSpans();

    if (g_failures == 0) {
        std::printf("ALL CHECKS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
