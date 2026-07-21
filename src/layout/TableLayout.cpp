/**
 * @file TableLayout.cpp
 * @brief CSS table layout engine with virtual scroll and O(visible-rows) materialization.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/TableLayout.h"

#include "html/DOMNode.h"
#include "css/ComputedStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace layout {

TableLayout::TableLayout(TextMeasurer& measurer) : measurer_(measurer) {}

std::vector<html::DOMNode*> TableLayout::collectRows(const html::DOMNode* tableNode) const {
    std::vector<html::DOMNode*> rows;
    if (!tableNode) return rows;

    for (html::DOMNode* child = tableNode->firstChild; child; child = child->nextSibling) {
        if (child->nodeType != html::NodeType::Element) continue;
        if (child->tagName == "table") continue;
        if (child->tagName == "tr") {
            rows.push_back(child);
        } else {
            auto nested = collectRows(child);
            rows.insert(rows.end(), nested.begin(), nested.end());
        }
    }
    return rows;
}

void TableLayout::collectColumns(const html::DOMNode* tableNode, std::vector<float>& colWidths) {
    colWidths.clear();
    if (!tableNode || !tableNode->firstChild) return;

    for (html::DOMNode* child = tableNode->firstChild; child; child = child->nextSibling) {
        if (child->nodeType != html::NodeType::Element) continue;
        if (child->tagName == "colgroup") {
            for (html::DOMNode* col = child->firstChild; col; col = col->nextSibling) {
                if (col->nodeType != html::NodeType::Element || col->tagName != "col") continue;
                float w = 0.0f;
                if (col->style) {
                    w = col->style->width;
                }
                colWidths.push_back(w);
            }
        } else if (child->tagName == "col") {
            float w = 0.0f;
            if (child->style) {
                w = child->style->width;
            }
            colWidths.push_back(w);
        }
    }
}

float TableLayout::measureCellMinWidth(const html::DOMNode* cellNode, float availableWidth) {
    if (!cellNode) return 0.0f;
    float maxW = 0.0f;
    for (html::DOMNode* child = cellNode->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            if (!child->textContent.empty()) {
                auto m = measurer_.measure(child->textContent, 16.0f, "Arial");
                if (m.width > maxW) maxW = m.width;
            }
        } else if (child->nodeType == html::NodeType::Element) {
            auto m = measurer_.measure("<" + child->tagName + ">", 16.0f, "Arial");
            if (m.width > maxW) maxW = m.width;
        }
    }
    if (cellNode->style) {
        maxW += cellNode->style->paddingLeft + cellNode->style->paddingRight +
                cellNode->style->borderLeft + cellNode->style->borderRight;
    }
    return maxW;
}

float TableLayout::measureCellContentHeight(const html::DOMNode* cellNode, float availableWidth) {
    if (!cellNode) return 0.0f;
    float maxH = 0.0f;
    for (html::DOMNode* child = cellNode->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            if (!child->textContent.empty()) {
                auto m = measurer_.measure(child->textContent, 16.0f, "Arial");
                if (m.height > maxH) maxH = m.height;
            }
        } else if (child->nodeType == html::NodeType::Element) {
            auto m = measurer_.measure("<" + child->tagName + ">", 16.0f, "Arial");
            if (m.height > maxH) maxH = m.height;
        }
    }
    if (cellNode->style) {
        maxH += cellNode->style->paddingTop + cellNode->style->paddingBottom +
                cellNode->style->borderTop + cellNode->style->borderBottom;
    }
    return std::max(0.0f, maxH);
}

void TableLayout::computeColumnWidthsAuto(const html::DOMNode* tableNode, float containingWidth, TableGeometry& geo) {
    std::vector<float> colWidths;
    collectColumns(tableNode, colWidths);

    auto rows = collectRows(tableNode);
    int maxCols = 0;
    for (const auto* row : rows) {
        int cols = 0;
        for (html::DOMNode* cell = row->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;
            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            cols += colspan;
        }
        if (cols > maxCols) maxCols = cols;
    }

    if (maxCols == 0) {
        geo.columnCount = 0;
        return;
    }

    geo.columnCount = maxCols;
    geo.columnWidths.assign(maxCols, 0.0f);

    if (!colWidths.empty()) {
        size_t copyCount = std::min(colWidths.size(), static_cast<size_t>(maxCols));
        for (size_t i = 0; i < copyCount; ++i) {
            geo.columnWidths[i] = colWidths[i];
        }
    }

    std::vector<float> minWidths(maxCols, 0.0f);
    for (const auto* row : rows) {
        int col = 0;
        for (html::DOMNode* cell = row->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;
            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            float cellMinW = measureCellMinWidth(cell, containingWidth / maxCols);
            float cellWidth = cellMinW / colspan;
            for (int i = 0; i < colspan && col + i < maxCols; ++i) {
                if (cellWidth > minWidths[col + i]) {
                    minWidths[col + i] = cellWidth;
                }
            }
            col += colspan;
        }
    }

    float totalMin = 0.0f;
    for (float w : minWidths) totalMin += w;

    float available = containingWidth;
    if (totalMin > available) {
        for (int i = 0; i < maxCols; ++i) {
            geo.columnWidths[i] = minWidths[i];
        }
    } else {
        float extra = available - totalMin;
        float distribute = extra / maxCols;
        for (int i = 0; i < maxCols; ++i) {
            geo.columnWidths[i] = minWidths[i] + distribute;
        }
    }

    for (int i = 0; i < maxCols; ++i) {
        if (geo.columnWidths[i] < 0.0f) geo.columnWidths[i] = 0.0f;
    }
}

void TableLayout::computeColumnWidthsFixed(const html::DOMNode* tableNode, TableGeometry& geo) {
    std::vector<float> colWidths;
    collectColumns(tableNode, colWidths);

    auto rows = collectRows(tableNode);
    int maxCols = 0;
    for (const auto* row : rows) {
        int cols = 0;
        for (html::DOMNode* cell = row->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;
            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            cols += colspan;
        }
        if (cols > maxCols) maxCols = cols;
    }

    if (maxCols == 0) {
        geo.columnCount = 0;
        return;
    }

    geo.columnCount = maxCols;
    geo.columnWidths.assign(maxCols, 0.0f);

    if (!colWidths.empty()) {
        float total = 0.0f;
        int count = 0;
        for (float w : colWidths) {
            if (w > 0.0f) {
                total += w;
                ++count;
            }
        }
        if (count > 0 && total > 0.0f) {
            float perCol = total / count;
            for (int i = 0; i < maxCols; ++i) {
                if (static_cast<size_t>(i) < colWidths.size() && colWidths[i] > 0.0f) {
                    geo.columnWidths[i] = colWidths[i];
                } else {
                    geo.columnWidths[i] = perCol;
                }
            }
        } else {
            float perCol = geo.totalWidth / maxCols;
            for (int i = 0; i < maxCols; ++i) {
                geo.columnWidths[i] = perCol;
            }
        }
    } else {
        float perCol = geo.totalWidth / maxCols;
        for (int i = 0; i < maxCols; ++i) {
            geo.columnWidths[i] = perCol;
        }
    }

    if (!rows.empty()) {
        const auto* firstRow = rows[0];
        int col = 0;
        for (html::DOMNode* cell = firstRow->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;
            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            if (cell->style && cell->style->width > 0.0f) {
                float cellW = cell->style->width;
                float perSpan = cellW / colspan;
                for (int i = 0; i < colspan && col + i < maxCols; ++i) {
                    geo.columnWidths[col + i] = perSpan;
                }
            }
            col += colspan;
        }
    }

    float total = 0.0f;
    for (float w : geo.columnWidths) total += w;
    if (total > 0.0f && total != geo.totalWidth) {
        float scale = geo.totalWidth / total;
        for (float& w : geo.columnWidths) w *= scale;
    }
}

void TableLayout::computeRowHeights(const html::DOMNode* tableNode, TableGeometry& geo) {
    auto rows = collectRows(tableNode);
    geo.rowCount = static_cast<int>(rows.size());
    geo.rowHeights.assign(geo.rowCount, 0.0f);

    std::vector<int> rowSpans(geo.columnCount, 0);

    for (int r = 0; r < geo.rowCount; ++r) {
        const auto* row = rows[r];
        int col = 0;
        for (html::DOMNode* cell = row->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;

            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            int rowspan = 1;
            auto rs = cell->getAttribute("rowspan");
            if (rs) rowspan = std::max(1, std::atoi(rs->c_str()));

            while (col < geo.columnCount && rowSpans[col] > 0) {
                ++col;
            }
            if (col >= geo.columnCount) break;

            float cellH = measureCellContentHeight(cell, (geo.totalWidth - (geo.columnCount > 0 ? geo.columnWidths[0] : 0)));
            float perRow = cellH / rowspan;
            for (int i = 0; i < rowspan && r + i < geo.rowCount; ++i) {
                if (perRow > geo.rowHeights[r + i]) {
                    geo.rowHeights[r + i] = perRow;
                }
            }

            if (rowspan > 1) {
                for (int i = 0; i < colspan && col + i < geo.columnCount; ++i) {
                    rowSpans[col + i] = rowspan - 1;
                }
            }
            col += colspan;
        }

        for (int c = 0; c < geo.columnCount; ++c) {
            if (rowSpans[c] > 0) --rowSpans[c];
        }
    }
}

void TableLayout::applySpans(TableGeometry& geo, const html::DOMNode* tableNode) {
    geo.columnOrigins.resize(geo.columnCount + 1);
    geo.rowOrigins.resize(geo.rowCount + 1);
    geo.columnOrigins[0] = 0;
    for (int i = 0; i < geo.columnCount; ++i) {
        geo.columnOrigins[i + 1] = geo.columnOrigins[i] + static_cast<int>(geo.columnWidths[i]);
    }
    geo.rowOrigins[0] = 0;
    for (int i = 0; i < geo.rowCount; ++i) {
        geo.rowOrigins[i + 1] = geo.rowOrigins[i] + static_cast<int>(geo.rowHeights[i]);
    }
}

TableGeometry TableLayout::computeGeometry(const html::DOMNode* tableNode, float containingWidth) {
    TableGeometry geo;
    if (!tableNode || !tableNode->firstChild) return geo;

    geo.totalWidth = containingWidth;

    bool isFixed = false;
    if (tableNode->style) {
        isFixed = (tableNode->style->tableLayout == css::ComputedStyle::FixedTable);
    }

    if (isFixed) {
        computeColumnWidthsFixed(tableNode, geo);
    } else {
        computeColumnWidthsAuto(tableNode, containingWidth, geo);
    }

    geo.totalWidth = 0.0f;
    for (float w : geo.columnWidths) geo.totalWidth += w;

    computeRowHeights(tableNode, geo);
    applySpans(geo, tableNode);

    geo.totalHeight = 0.0f;
    for (float h : geo.rowHeights) geo.totalHeight += h;

    return geo;
}

float TableLayout::getColumnWidth(const TableGeometry& geo, int colIndex, int colspan) const {
    float w = 0.0f;
    for (int i = 0; i < colspan && colIndex + i < geo.columnCount; ++i) {
        if (colIndex + i < static_cast<int>(geo.columnWidths.size())) {
            w += geo.columnWidths[colIndex + i];
        }
    }
    return w;
}

int TableLayout::findFirstVisibleRow(const TableGeometry& geo, float viewportTop) const {
    for (int i = 0; i < geo.rowCount; ++i) {
        if (geo.rowOrigins[i + 1] > viewportTop) return i;
    }
    return geo.rowCount - 1;
}

int TableLayout::findLastVisibleRow(const TableGeometry& geo, float viewportBottom) const {
    for (int i = geo.rowCount - 1; i >= 0; --i) {
        if (geo.rowOrigins[i] < viewportBottom) return i;
    }
    return 0;
}

std::unique_ptr<LayoutNode> TableLayout::buildRowBox(html::DOMNode* rowNode, const TableGeometry& geo, float startY, float containingWidth) {
    if (!rowNode) return nullptr;

    auto box = std::make_unique<LayoutNode>();
    box->domNode = rowNode;
    box->display = css::ComputedStyle::TableRow;
    box->position = css::ComputedStyle::Static;
    box->x = 0.0f;
    box->y = startY;
    box->width = geo.totalWidth;
    box->height = 0.0f;

    return box;
}

std::unique_ptr<LayoutNode> TableLayout::buildCellBox(html::DOMNode* cellNode, const TableGeometry& geo, int rowIndex, int colIndex, int colspan, int rowspan, float startY, float containingWidth) {
    if (!cellNode) return nullptr;

    auto box = std::make_unique<LayoutNode>();
    box->domNode = cellNode;
    box->display = css::ComputedStyle::TableCell;
    box->position = css::ComputedStyle::Static;
    box->x = geo.columnOrigins[colIndex];
    box->y = startY;
    box->width = getColumnWidth(geo, colIndex, colspan);
    box->height = 0.0f;

    for (int i = rowIndex; i < rowIndex + rowspan && i < geo.rowCount; ++i) {
        box->height += geo.rowHeights[i];
    }

    float cellContentWidth = box->width;
    if (cellNode->style) {
        cellContentWidth -= cellNode->style->paddingLeft + cellNode->style->paddingRight +
                           cellNode->style->borderLeft + cellNode->style->borderRight;
    }
    if (cellContentWidth < 0.0f) cellContentWidth = 0.0f;

    for (html::DOMNode* child = cellNode->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            if (!child->textContent.empty()) {
                auto textBox = std::make_unique<LayoutNode>();
                textBox->domNode = child;
                textBox->display = css::ComputedStyle::Inline;
                textBox->position = css::ComputedStyle::Static;
                auto m = measurer_.measure(child->textContent, 16.0f, "Arial");
                textBox->width = m.width;
                textBox->height = m.height;
                textBox->x = box->contentX();
                textBox->y = box->contentY();
                box->children.push_back(std::move(textBox));
            }
        } else if (child->nodeType == html::NodeType::Element) {
            auto childBox = std::make_unique<LayoutNode>();
            childBox->domNode = child;
            childBox->display = css::ComputedStyle::Inline;
            childBox->position = css::ComputedStyle::Static;
            auto m = measurer_.measure("<" + child->tagName + ">", 16.0f, "Arial");
            childBox->width = m.width;
            childBox->height = m.height;
            childBox->x = box->contentX();
            childBox->y = box->contentY();
            box->children.push_back(std::move(childBox));
        }
    }

    return box;
}

std::vector<std::unique_ptr<LayoutNode>> TableLayout::materializeVisibleRows(
    const html::DOMNode* tableNode,
    const TableGeometry& geo,
    float viewportTop,
    float viewportBottom,
    float containingWidth,
    float startY) {

    std::vector<std::unique_ptr<LayoutNode>> rows;
    if (!tableNode || geo.rowCount == 0) return rows;

    auto domRows = collectRows(tableNode);
    int first = findFirstVisibleRow(geo, viewportTop);
    int last = findLastVisibleRow(geo, viewportBottom);
    first = std::max(0, first - 1);
    last = std::min(geo.rowCount - 1, last + 1);

    std::vector<int> rowSpans(geo.columnCount, 0);

    for (int r = first; r <= last; ++r) {
        if (r >= static_cast<int>(domRows.size())) break;

        auto rowBox = buildRowBox(domRows[r], geo, geo.rowOrigins[r] + startY, containingWidth);
        if (!rowBox) continue;

        int col = 0;
        for (html::DOMNode* cell = domRows[r]->firstChild; cell; cell = cell->nextSibling) {
            if (cell->nodeType != html::NodeType::Element) continue;
            if (cell->tagName != "td" && cell->tagName != "th") continue;

            int colspan = 1;
            auto cs = cell->getAttribute("colspan");
            if (cs) colspan = std::max(1, std::atoi(cs->c_str()));
            int rowspan = 1;
            auto rs = cell->getAttribute("rowspan");
            if (rs) rowspan = std::max(1, std::atoi(rs->c_str()));

            while (col < geo.columnCount && rowSpans[col] > 0) {
                ++col;
            }
            if (col >= geo.columnCount) break;

            auto cellBox = buildCellBox(cell, geo, r, col, colspan, rowspan, geo.rowOrigins[r] + startY, containingWidth);
            if (cellBox) {
                cellBox->x = geo.columnOrigins[col];
                rowBox->children.push_back(std::move(cellBox));
            }

            if (rowspan > 1) {
                for (int i = 0; i < colspan && col + i < geo.columnCount; ++i) {
                    rowSpans[col + i] = rowspan - 1;
                }
            }
            col += colspan;
        }

        for (int c = 0; c < geo.columnCount; ++c) {
            if (rowSpans[c] > 0) --rowSpans[c];
        }

        rowBox->height = geo.rowHeights[r];
        rows.push_back(std::move(rowBox));
    }

    return rows;
}

} // namespace layout
