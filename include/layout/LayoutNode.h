/**
 * @file LayoutNode.h
 * @brief Positioned layout tree node.
 * @details Each LayoutNode represents one CSS box after the layout pass has
 *          computed its geometry (x, y, width, height) in viewport coordinates.
 *          The tree mirrors the DOM but drops display:none nodes and flattens
 *          anonymous block boxes.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_LAYOUTNODE_H
#define LAYOUT_LAYOUTNODE_H

#include <memory>
#include <vector>

#include "css/ComputedStyle.h"
#include "html/DOMNode.h"

namespace layout {

struct LayoutNode {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    const html::DOMNode* domNode{nullptr};

    std::vector<std::unique_ptr<LayoutNode>> children;

    css::ComputedStyle::Display display{css::ComputedStyle::Block};
    css::ComputedStyle::Position position{css::ComputedStyle::Static};

    float contentX() const {
        return x + static_cast<float>(domNode ? domNode->style->paddingLeft : 0) +
               static_cast<float>(domNode ? domNode->style->borderLeft : 0);
    }
    float contentY() const {
        return y + static_cast<float>(domNode ? domNode->style->paddingTop : 0) +
               static_cast<float>(domNode ? domNode->style->borderTop : 0);
    }
    float contentWidth() const {
        float border = domNode ? domNode->style->borderLeft + domNode->style->borderRight : 0.0f;
        float padding = domNode ? domNode->style->paddingLeft + domNode->style->paddingRight : 0.0f;
        return (width - border - padding > 0.0f) ? (width - border - padding) : 0.0f;
    }
    float contentHeight() const {
        float border = domNode ? domNode->style->borderTop + domNode->style->borderBottom : 0.0f;
        float padding = domNode ? domNode->style->paddingTop + domNode->style->paddingBottom : 0.0f;
        return (height - border - padding > 0.0f) ? (height - border - padding) : 0.0f;
    }
};

} // namespace layout

#endif // LAYOUT_LAYOUTNODE_H
