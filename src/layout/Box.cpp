/**
 * @file Box.cpp
 * @brief Virtual-scroll table layout engine implementation.
 * @details See layout/Box.h for the design.  The engine keeps layout memory
 *          bounded to O(visibleRows x columns) by only materialising LayoutBox
 *          objects for rows intersecting the active viewport, while row heights
 *          are measured once from a template row and replicated (virtual scroll).
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/Box.h"

#include "html/DOMNode.h"
#include "css/ComputedStyle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace layout {

namespace {

/// @brief Lower-case a string (attribute keys are lower-cased on parse).
std::string toLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return s;
}

/// @brief Parse the first integer found in a string (e.g. "2", "120px", "3").
int parseIntAttr(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size()) return 0;
    int v = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        v = v * 10 + (s[i] - '0');
        ++i;
    }
    return v;
}

/// @brief True if the node is a <table> element.
bool isTable(const html::DOMNode* n) {
    return n && n->nodeType == html::NodeType::Element && toLower(n->tagName) == "table";
}

/// @brief True if the node is a <tr> element.
bool isRow(const html::DOMNode* n) {
    return n && n->nodeType == html::NodeType::Element && toLower(n->tagName) == "tr";
}

/// @brief True if the node is a <td> or <th> cell element.
bool isCell(const html::DOMNode* n) {
    if (!n || n->nodeType != html::NodeType::Element) return false;
    std::string t = toLower(n->tagName);
    return t == "td" || t == "th";
}

/// @brief Sum of horizontal padding/border for a cell (kept minimal).
float cellVerticalExtent(const html::DOMNode* cell) {
    if (!cell || !cell->style) return 0.0f;
    const auto& s = *cell->style;
    return s.paddingTop + s.paddingBottom + s.borderTop + s.borderBottom;
}

} // namespace

// ── prepare ───────────────────────────────────────────────────────────────────

bool TableLayoutEngine::prepare(html::DOMNode* table) {
    table_ = table;
    rowNodes_.clear();
    colX_.clear();
    colWidth_.clear();

    if (!isTable(table)) return false;

    collectRows(table, rowNodes_);
    rowCount_ = static_cast<int>(rowNodes_.size());
    if (rowCount_ == 0) return false;

    computeColumnWidths();
    if (columnCount_ <= 0) return false;

    measureRowHeight();

    // Allocate per-pass state sized to the column count (NOT the row count).
    spanState_.assign(static_cast<size_t>(columnCount_), SpanInfo{});
    colCovered_.assign(static_cast<size_t>(columnCount_), false);
    occupiedRow_.assign(static_cast<size_t>(columnCount_), false);

    return true;
}

void TableLayoutEngine::collectRows(html::DOMNode* node, std::vector<html::DOMNode*>& out) {
    if (!node) return;
    for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        if (isRow(child)) {
            out.push_back(child);
        } else if (isTable(child)) {
            // Do not descend into nested tables.
            continue;
        } else if (child->nodeType == html::NodeType::Element) {
            collectRows(child, out);
        }
    }
}

void TableLayoutEngine::computeColumnWidths() {
    // 1) Try an explicit <colgroup> with <col> elements.
    std::vector<float> widths;
    for (html::DOMNode* child = table_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType != html::NodeType::Element) continue;
        if (toLower(child->tagName) != "colgroup") continue;
        for (html::DOMNode* col = child->firstChild; col; col = col->nextSibling) {
            if (col->nodeType != html::NodeType::Element) continue;
            if (toLower(col->tagName) != "col") continue;
            int span = parseIntAttr(col->attributes["span"]);
            if (span < 1) span = 1;
            float w = 0.0f;
            auto it = col->attributes.find("width");
            if (it != col->attributes.end()) w = static_cast<float>(parseIntAttr(it->second));
            if (w <= 0.0f) w = 100.0f;  // default column width
            for (int i = 0; i < span; ++i) widths.push_back(w);
        }
    }

    // 2) Otherwise derive column count/widths from the first row's cells.
    if (widths.empty() && !rowNodes_.empty()) {
        html::DOMNode* firstRow = rowNodes_[0];
        int col = 0;
        for (html::DOMNode* c = firstRow->firstChild; c; c = c->nextSibling) {
            if (!isCell(c)) continue;
            int cs = parseIntAttr(c->attributes["colspan"]);
            if (cs < 1) cs = 1;
            float w = 0.0f;
            auto it = c->attributes.find("width");
            if (it != c->attributes.end()) w = static_cast<float>(parseIntAttr(it->second));
            if (w <= 0.0f) w = 100.0f;
            for (int i = 0; i < cs; ++i) widths.push_back(w);
            col += cs;
        }
    }

    if (widths.empty()) {
        columnCount_ = 0;
        return;
    }

    columnCount_ = static_cast<int>(widths.size());
    colWidth_ = std::move(widths);

    colX_.resize(static_cast<size_t>(columnCount_) + 1);
    float x = 0.0f;
    colX_[0] = 0.0f;
    for (int c = 0; c < columnCount_; ++c) {
        x += colWidth_[c];
        colX_[c + 1] = x;
    }
    totalWidth_ = x;
}

void TableLayoutEngine::measureRowHeight() {
    rowHeight_ = 22.0f;  // sensible default before any measurement

    if (rowNodes_.empty()) return;
    html::DOMNode* firstRow = rowNodes_[0];

    // Measure from the first cell's resolved style: line box + vertical chrome.
    float maxLine = 0.0f;
    for (html::DOMNode* c = firstRow->firstChild; c; c = c->nextSibling) {
        if (!isCell(c)) continue;
        float h = 0.0f;
        if (c->style) {
            const auto& s = *c->style;
            h = s.fontSize * s.lineHeight + cellVerticalExtent(c);
        }
        if (h > maxLine) maxLine = h;
    }

    if (maxLine > 0.0f) rowHeight_ = maxLine;
}

// ── layout (virtual scroll + viewport culling) ────────────────────────────────

void TableLayoutEngine::layout(double viewportTop, double viewportHeight) {
    boxes_.clear();
    if (rowCount_ == 0 || columnCount_ == 0) return;

    firstRow_ = static_cast<int>(std::floor(viewportTop / rowHeight_));
    int lastRow = static_cast<int>(std::ceil((viewportTop + viewportHeight) / rowHeight_));
    if (firstRow_ < 0) firstRow_ = 0;
    if (lastRow > rowCount_) lastRow = rowCount_;
    if (firstRow_ >= lastRow) return;

    const int seedStart = std::max(0, firstRow_ - kSeedRows);

    // Reservoir cap: never materialise more than (window + seed + overscan) rows
    // worth of cells.  This is the O(visibleRows x columns) guarantee.
    const int bandRows = (lastRow - firstRow_) + kSeedRows + kOverscan;
    boxCapacity_ = static_cast<size_t>(bandRows) * static_cast<size_t>(columnCount_);
    if (boxes_.capacity() < boxCapacity_) boxes_.reserve(boxCapacity_);

    // Seed span state from the rows just above the visible window (no boxes).
    for (int r = seedStart; r < firstRow_; ++r) {
        processRow(r, false);
    }
    // Materialise boxes for every row intersecting the viewport.
    for (int r = firstRow_; r < lastRow; ++r) {
        processRow(r, true);
    }
}

void TableLayoutEngine::processRow(int r, bool emit) {
    // 1) Expire spans that no longer cover this row.
    for (int c = 0; c < columnCount_; ++c) {
        SpanInfo& sp = spanState_[c];
        if (sp.active && sp.endRow <= r) {
            for (int k = c; k < c + sp.colSpan && k < columnCount_; ++k) {
                colCovered_[k] = false;
            }
            sp.active = false;
        }
    }

    // 2) Reset this row's occupancy and re-mark columns covered by active spans.
    std::fill(occupiedRow_.begin(), occupiedRow_.end(), false);
    for (int c = 0; c < columnCount_; ++c) {
        SpanInfo& sp = spanState_[c];
        if (!sp.active) continue;
        for (int k = c; k < c + sp.colSpan && k < columnCount_; ++k) {
            colCovered_[k] = true;
            occupiedRow_[k] = true;
        }
        // Emit a spanning cell when it first becomes visible in this window.
        if (emit && !sp.emitted) {
            if (sp.anchorRow == r) {
                emitSpan(sp, r);
                sp.emitted = true;
            } else if (sp.anchorRow < firstRow_) {
                emitSpan(sp, r);  // span started above the window: draw its clipped top.
                sp.emitted = true;
            }
        }
    }

    // 3) Place this row's own cells.
    html::DOMNode* tr = (r < static_cast<int>(rowNodes_.size())) ? rowNodes_[r] : nullptr;
    if (tr) {
        int col = 0;
        for (html::DOMNode* cell = tr->firstChild; cell; cell = cell->nextSibling) {
            if (!isCell(cell)) continue;

            // Skip columns already filled by a span or an earlier cell.
            while (col < columnCount_ && (occupiedRow_[col] || colCovered_[col])) ++col;
            if (col >= columnCount_) break;

            int cs = parseIntAttr(cell->attributes["colspan"]);
            int rs = parseIntAttr(cell->attributes["rowspan"]);
            if (cs < 1) cs = 1;
            if (rs < 1) rs = 1;
            if (col + cs > columnCount_) cs = columnCount_ - col;

            bool header = (toLower(cell->tagName) == "th");

            if (emit) emitCell(cell, r, col, cs, rs, header);

            for (int k = col; k < col + cs && k < columnCount_; ++k) {
                occupiedRow_[k] = true;
            }
            if (rs > 1) {
                SpanInfo& sp = spanState_[col];
                sp.active = true;
                sp.endRow = r + rs;
                sp.anchorRow = r;
                sp.colSpan = cs;
                sp.node = cell;
                sp.header = header;
                sp.emitted = false;
                for (int k = col; k < col + cs && k < columnCount_; ++k) {
                    colCovered_[k] = true;
                }
            }
            col += cs;
        }
    } else if (emit) {
        // Synthetic (spec) table with no DOM cell nodes: emit one uniform cell
        // per free column so the visible window is still materialised.
        for (int col = 0; col < columnCount_; ++col) {
            if (occupiedRow_[col] || colCovered_[col]) continue;
            emitCell(nullptr, r, col, 1, 1, false);
            occupiedRow_[col] = true;
        }
    }
}

void TableLayoutEngine::emitCell(html::DOMNode* cell, int row, int col,
                                 int colSpan, int rowSpan, bool header) {
    if (boxes_.size() >= boxCapacity_) return;  // hard memory cap
    LayoutBox b;
    b.node = cell;
    b.row = row;
    b.col = col;
    b.colSpan = colSpan;
    b.rowSpan = rowSpan;
    b.isHeader = header;
    b.x = colX_[col];
    b.y = static_cast<float>(row) * rowHeight_;
    b.width = colX_[col + colSpan] - colX_[col];
    b.height = static_cast<float>(rowSpan) * rowHeight_;
    boxes_.push_back(b);
}

void TableLayoutEngine::emitSpan(const SpanInfo& sp, int row) {
    if (boxes_.size() >= boxCapacity_) return;  // hard memory cap
    LayoutBox b;
    b.node = sp.node;
    b.row = sp.anchorRow;
    b.col = 0;  // recomputed below from the span's anchor column
    // Find the anchor column for this span.
    for (int c = 0; c < columnCount_; ++c) {
        if (&spanState_[c] == &sp) { b.col = c; break; }
    }
    b.colSpan = sp.colSpan;
    b.rowSpan = sp.endRow - sp.anchorRow;
    b.isHeader = sp.header;
    b.x = colX_[b.col];
    b.y = static_cast<float>(row) * rowHeight_;
    b.height = static_cast<float>(sp.endRow - row) * rowHeight_;
    b.width = colX_[b.col + sp.colSpan] - colX_[b.col];
    boxes_.push_back(b);
}

// ── synthetic construction ─────────────────────────────────────────────────────

void TableLayoutEngine::prepareFromSpec(const Spec& spec) {
    table_ = nullptr;
    rowNodes_.clear();
    rowCount_ = spec.rows;
    columnCount_ = spec.cols;
    rowHeight_ = spec.rowHeight;
    colWidth_.assign(static_cast<size_t>(columnCount_), spec.colWidth);
    colX_.resize(static_cast<size_t>(columnCount_) + 1);
    float x = 0.0f;
    colX_[0] = 0.0f;
    for (int c = 0; c < columnCount_; ++c) {
        x += colWidth_[c];
        colX_[c + 1] = x;
    }
    totalWidth_ = x;
    spanState_.assign(static_cast<size_t>(columnCount_), SpanInfo{});
    colCovered_.assign(static_cast<size_t>(columnCount_), false);
    occupiedRow_.assign(static_cast<size_t>(columnCount_), false);
}

// ── memory accounting ──────────────────────────────────────────────────────────

size_t TableLayoutEngine::layoutMemoryBytes() const {
    size_t bytes = 0;
    bytes += boxes_.capacity() * sizeof(LayoutBox);
    bytes += colX_.capacity() * sizeof(float);
    bytes += colWidth_.capacity() * sizeof(float);
    bytes += rowNodes_.capacity() * sizeof(html::DOMNode*);
    bytes += spanState_.capacity() * sizeof(SpanInfo);
    bytes += colCovered_.capacity() * sizeof(bool);
    bytes += occupiedRow_.capacity() * sizeof(bool);
    return bytes;
}

// ── Box facade ─────────────────────────────────────────────────────────────────

const TableLayoutEngine* Box::table(int i) const {
    if (i < 0 || i >= static_cast<int>(tables_.size())) return nullptr;
    return tables_[i].engine.get();
}

TableLayoutEngine* Box::table(int i) {
    if (i < 0 || i >= static_cast<int>(tables_.size())) return nullptr;
    return tables_[i].engine.get();
}

void Box::layout(html::DOMNode* root) {
    tables_.clear();
    if (!root) return;

    // Collect every <table> in the subtree (nested tables handled independently).
    std::vector<html::DOMNode*> tables;
    std::function<void(html::DOMNode*)> walk = [&](html::DOMNode* n) {
        if (!n) return;
        if (isTable(n)) tables.push_back(n);
        for (html::DOMNode* c = n->firstChild; c; c = c->nextSibling) {
            if (isTable(c)) {
                tables.push_back(c);
                continue;  // don't descend into it here
            }
            walk(c);
        }
    };
    walk(root);

    tables_.resize(tables.size());
    for (size_t i = 0; i < tables.size(); ++i) {
        tables_[i].node = tables[i];
        tables_[i].engine = std::make_unique<TableLayoutEngine>();
        if (tables_[i].engine->prepare(tables[i])) {
            tables_[i].engine->layout(viewportTop_, viewportHeight_);
        }
    }
}

void Box::setViewport(double scrollY, double viewportHeight) {
    viewportTop_ = scrollY;
    viewportHeight_ = viewportHeight;
    for (auto& h : tables_) {
        if (h.node) h.engine->layout(viewportTop_, viewportHeight_);
    }
}

} // namespace layout
