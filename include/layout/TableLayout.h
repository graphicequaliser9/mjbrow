/**
 * @file layout/TableLayout.h
 * @brief Table layout engine.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_TABLELAYOUT_H
#define LAYOUT_TABLELAYOUT_H

#include <vector>
#include "layout/LayoutNode.h"
#include "html/DOMNode.h"

namespace layout {

struct TableGeometry {
    std::vector<float> columnWidths;
    float totalWidth{0.0f};
};

float measureCellContentHeight(html::DOMNode* cell, float availableWidth);
void computeRowHeights(html::DOMNode* table, TableGeometry& geo);

} // namespace layout

#endif // LAYOUT_TABLELAYOUT_H
