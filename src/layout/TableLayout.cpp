#include "layout/TableLayout.h"
#include "html/DOMNode.h"

namespace layout {

float measureCellContentHeight(html::DOMNode* cell, float availableWidth) {
    if (!cell) return 0.0f;

    float contentWidth = availableWidth;
    if (cell->style) {
        contentWidth -= cell->style->paddingLeft + cell->style->paddingRight
                      + cell->style->borderLeft  + cell->style->borderRight;
    }
    if (contentWidth < 0.0f) contentWidth = 0.0f;

    float height = 0.0f;
    for (html::DOMNode* child = cell->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            const std::string& text = child->textContent;
            if (!text.empty()) {
                size_t lines = 1;
                for (char ch : text) {
                    if (ch == '\n') ++lines;
                }
                float fontSize = 16.0f;
                float lineHeight = 1.2f;
                if (child->style) {
                    fontSize = child->style->fontSize;
                    lineHeight = child->style->lineHeight;
                }
                float textHeight = lines * fontSize * lineHeight;
                if (textHeight > height) height = textHeight;
            }
        }
    }

    if (cell->style) {
        height += cell->style->paddingTop + cell->style->paddingBottom
                + cell->style->borderTop  + cell->style->borderBottom
                + cell->style->marginTop   + cell->style->marginBottom;
    }

    return height;
}

void computeRowHeights(html::DOMNode* table, TableGeometry& geo) {
    if (!table || !table->style) return;

    html::DOMNode* row = table->firstChild;
    while (row) {
        if (row->nodeType == html::NodeType::Element && row->tagName == "tr") {
            float rowMaxHeight = 0.0f;
            html::DOMNode* cell = row->firstChild;
            int colIndex = 0;
            while (cell) {
                if (cell->nodeType == html::NodeType::Element) {
                    int colspan = 1;
                    auto it = cell->attributes.find("colspan");
                    if (it != cell->attributes.end()) {
                        colspan = std::atoi(it->second.c_str());
                        if (colspan < 1) colspan = 1;
                    }

                    float cellWidth = 0.0f;
                    for (int i = colIndex; i < colIndex + colspan && i < (int)geo.columnWidths.size(); ++i) {
                        cellWidth += geo.columnWidths[i];
                    }

                    float h = measureCellContentHeight(cell, cellWidth);
                    if (h > rowMaxHeight) rowMaxHeight = h;
                    colIndex += colspan;
                }
                cell = cell->nextSibling;
            }

            if (row->style) {
                row->style->height = rowMaxHeight;
            }
        }
        row = row->nextSibling;
    }
}

} // namespace layout
