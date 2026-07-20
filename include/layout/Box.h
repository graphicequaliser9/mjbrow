/**
 * @file Box.h
 * @brief CSS layout engine entry point and box generation.
 * @details Walks a DOM subtree, generates layout boxes for rendered nodes,
 *          then delegates to the block / inline layout passes.  Returns a
 *          positioned LayoutNode tree.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

#include <memory>
#include <vector>

#include "html/DOMNode.h"
#include "layout/LayoutNode.h"

namespace layout {

class Box {
public:
    Box();
    ~Box();

    std::vector<std::unique_ptr<LayoutNode>> layout(html::DOMNode* root);

private:
    std::unique_ptr<LayoutNode> buildBox(html::DOMNode* node, float containingWidth, float containingHeight);
    void assignComputedStyles(html::DOMNode* node);
    void layoutBlock(LayoutNode* box, float containingWidth, float containingHeight);
    void layoutInline(LayoutNode* box, float containingWidth, float containingHeight);
    float measureTextWidth(const std::string& text, float fontSize, const std::string& fontFamily);
    float measureTextHeight(float fontSize, const std::string& fontFamily);
    void collapseMargins(float& topMargin, float& bottomMargin, const LayoutNode* parent);
};

} // namespace layout

#endif // LAYOUT_BOX_H
