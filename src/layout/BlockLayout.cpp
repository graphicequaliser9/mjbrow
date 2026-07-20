/**
 * @file BlockLayout.cpp
 * @brief Block formatting context and margin collapsing implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/BlockLayout.h"

#include "layout/InlineLayout.h"
#include "layout/TextMeasurer.h"

#include "html/DOMNode.h"
#include "css/ComputedStyle.h"

#include <algorithm>
#include <cmath>

namespace layout {

void BlockLayout::layout(LayoutNode* root, float containingWidth, float containingHeight) {
    if (!root) return;
    layoutBlockChildren(root, containingWidth, containingHeight);
}

void BlockLayout::layoutBlockChildren(LayoutNode* parent, float containingWidth, float containingHeight) {
    if (!parent) return;

    float cursorY = 0.0f;
    float maxChildWidth = 0.0f;

    std::vector<LayoutNode*> inlineChildren;
    std::vector<LayoutNode*> blockChildren;

    for (auto& child : parent->children) {
        if (!child || !child->domNode) continue;
        if (child->domNode->nodeType == html::NodeType::Text) {
            inlineChildren.push_back(child.get());
        } else if (child->domNode->style &&
                   (child->display == css::ComputedStyle::Inline || child->display == css::ComputedStyle::InlineBlock)) {
            inlineChildren.push_back(child.get());
        } else {
            blockChildren.push_back(child.get());
        }
    }

    for (LayoutNode* child : blockChildren) {
        float usedWidth = computeUsedWidth(child, containingWidth);
        float usedHeight = computeUsedHeight(child);
        float marginTop = verticalMargin(child);
        float marginBottom = verticalMargin(child);

        child->x = parent->contentX();
        child->y = parent->contentY() + cursorY + marginTop;
        child->width = usedWidth;

        if (child->children.empty()) {
            child->height = usedHeight;
        } else {
            layoutBlockChildren(child, child->contentWidth(), containingHeight);
            if (usedHeight > 0.0f) {
                child->height = usedHeight;
            }
        }

        cursorY += marginTop + child->height + marginBottom;
        float childRight = child->x + child->width - parent->x;
        if (childRight > maxChildWidth) maxChildWidth = childRight;
    }

    if (!inlineChildren.empty()) {
        float inlineStartY = cursorY;
        float inlineHeight = 0.0f;
        float inlineWidth = 0.0f;

        TextMeasurer measurer;
        InlineLayout inlineLayout(measurer);
        std::vector<InlineLayout::LineBox> lines;
        inlineLayout.buildLineBoxesForChildren(parent, inlineChildren, containingWidth, lines);
        inlineLayout.positionInlineChildren(parent, inlineChildren, lines, parent->contentY() + cursorY);

        for (const auto& line : lines) {
            inlineHeight += line.height;
            if (line.width > inlineWidth) inlineWidth = line.width;
        }

        if (inlineHeight > 0.0f) {
            cursorY += inlineHeight;
        }
        if (inlineWidth > maxChildWidth) maxChildWidth = inlineWidth;
    }

    float totalHeight = cursorY;
    if (totalHeight > parent->height) {
        parent->height = totalHeight;
    }
    if (maxChildWidth > parent->width - (parent->domNode ? parent->domNode->style->borderLeft + parent->domNode->style->borderRight + parent->domNode->style->paddingLeft + parent->domNode->style->paddingRight : 0.0f)) {
        float extra = maxChildWidth + (parent->domNode ? parent->domNode->style->borderLeft + parent->domNode->style->borderRight + parent->domNode->style->paddingLeft + parent->domNode->style->paddingRight : 0.0f);
        if (extra > parent->width) parent->width = extra;
    }
}

float BlockLayout::computeUsedWidth(const LayoutNode* box, float containingWidth) {
    if (!box || !box->domNode || !box->domNode->style) return containingWidth;
    const css::ComputedStyle* style = box->domNode->style;
    if (style->width > 0.0f) {
        return style->width + style->borderLeft + style->borderRight + style->paddingLeft + style->paddingRight;
    }
    return containingWidth;
}

float BlockLayout::computeUsedHeight(const LayoutNode* box) {
    if (!box || !box->domNode || !box->domNode->style) return 0.0f;
    return box->domNode->style->height;
}

float BlockLayout::horizontalMargin(const LayoutNode* box) {
    if (!box || !box->domNode || !box->domNode->style) return 0.0f;
    return box->domNode->style->marginLeft + box->domNode->style->marginRight;
}

float BlockLayout::verticalMargin(const LayoutNode* box) {
    if (!box || !box->domNode || !box->domNode->style) return 0.0f;
    return box->domNode->style->marginTop;
}

float BlockLayout::borderBoxWidth(const LayoutNode* box) {
    if (!box || !box->domNode || !box->domNode->style) return 0.0f;
    return box->domNode->style->borderLeft + box->domNode->style->borderRight;
}

float BlockLayout::borderBoxHeight(const LayoutNode* box) {
    if (!box || !box->domNode || !box->domNode->style) return 0.0f;
    return box->domNode->style->borderTop + box->domNode->style->borderBottom;
}

void BlockLayout::collapseAdjacentMargins(float& a, float& b) {
    float max = (a > b) ? a : b;
    a = max;
    b = max;
}

void BlockLayout::layoutInlineBlock(LayoutNode* box, float containingWidth, float containingHeight) {
    if (!box) return;
    box->width = containingWidth;
    box->height = containingHeight;
}

} // namespace layout
