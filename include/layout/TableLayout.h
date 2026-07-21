/**
 * @file TableLayout.h
 * @brief CSS table layout engine with virtual scroll support.
 * @details Implements CSS 2.1 table layout algorithms (auto / fixed),
 *          column sizing from <colgroup>/<col> and first-row cells,
 *          colspan/rowspan support, and virtual scrolling so that only
 *          visible rows materialise LayoutNodes.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_TABLELAYOUT_H
#define LAYOUT_TABLELAYOUT_H

#include <memory>
#include <vector>

#include "layout/LayoutNode.h"
#include "layout/TextMeasurer.h"

namespace html { class DOMNode; }

namespace layout {

struct TableGeometry {
    std::vector<float> columnWidths;
    std::vector<float> rowHeights;
    std::vector<int> columnOrigins;
    std::vector<int> rowOrigins;
    float totalWidth{0.0f};
    float totalHeight{0.0f};
    int columnCount{0};
    int rowCount{0};
};

class TableLayout {
public:
    TableLayout(TextMeasurer& measurer);
    ~TableLayout() = default;

    TableGeometry computeGeometry(const html::DOMNode* tableNode, float containingWidth);

    std::vector<std::unique_ptr<LayoutNode>> materializeVisibleRows(
        const html::DOMNode* tableNode,
        const TableGeometry& geometry,
        float viewportTop,
        float viewportBottom,
        float containingWidth,
        float startY);

private:
    TextMeasurer& measurer_;

    std::vector<html::DOMNode*> collectRows(const html::DOMNode* tableNode) const;
    void collectColumns(const html::DOMNode* tableNode, std::vector<float>& colWidths);
    void computeColumnWidthsAuto(const html::DOMNode* tableNode, float containingWidth, TableGeometry& geo);
    void computeColumnWidthsFixed(const html::DOMNode* tableNode, TableGeometry& geo);
    float measureCellMinWidth(const html::DOMNode* cellNode, float availableWidth);
    float measureCellContentHeight(const html::DOMNode* cellNode, float availableWidth);
    void computeRowHeights(const html::DOMNode* tableNode, TableGeometry& geo);
    void applySpans(TableGeometry& geo, const html::DOMNode* tableNode);
    std::unique_ptr<LayoutNode> buildRowBox(html::DOMNode* rowNode, const TableGeometry& geo, float startY, float containingWidth);
    std::unique_ptr<LayoutNode> buildCellBox(html::DOMNode* cellNode, const TableGeometry& geo, int rowIndex, int colIndex, int colspan, int rowspan, float startY, float containingWidth);
    float getColumnWidth(const TableGeometry& geo, int colIndex, int colspan) const;
    int findFirstVisibleRow(const TableGeometry& geo, float viewportTop) const;
    int findLastVisibleRow(const TableGeometry& geo, float viewportBottom) const;
};

} // namespace layout

#endif // LAYOUT_TABLELAYOUT_H
