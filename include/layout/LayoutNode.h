#ifndef LAYOUT_LAYOUTNODE_H
#define LAYOUT_LAYOUTNODE_H

#include "html/DOMNode.h"

namespace layout {

class LayoutNode {
public:
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    float marginTop{0.0f};
    float marginBottom{0.0f};
    bool isBlock{false};
    LayoutNode* parent{nullptr};
    LayoutNode* firstChild{nullptr};
    LayoutNode* lastChild{nullptr};
    LayoutNode* nextSibling{nullptr};
    LayoutNode* prevSibling{nullptr};
    html::DOMNode* domNode{nullptr};

    int contentX() const {
        if (!domNode || !domNode->style) return x;
        return x + static_cast<int>(domNode->style->paddingLeft + domNode->style->borderLeft);
    }

    int contentY() const {
        if (!domNode || !domNode->style) return y;
        return y + static_cast<int>(domNode->style->paddingTop + domNode->style->borderTop);
    }

    int contentWidth() const {
        if (!domNode || !domNode->style) return width;
        float w = width - domNode->style->paddingLeft - domNode->style->paddingRight
                       - domNode->style->borderLeft  - domNode->style->borderRight;
        return w > 0 ? static_cast<int>(w) : 0;
    }

    int contentHeight() const {
        if (!domNode || !domNode->style) return height;
        float h = height - domNode->style->paddingTop - domNode->style->paddingBottom
                        - domNode->style->borderTop  - domNode->style->borderBottom;
        return h > 0 ? static_cast<int>(h) : 0;
    }
};

} // namespace layout

#endif
