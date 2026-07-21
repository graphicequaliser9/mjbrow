/**
 * @file Box.h
 * @brief CSS layout engine entry point and box generation.
 * @details Walks a DOM subtree, generates layout boxes for rendered nodes,
 *          then delegates to the block / inline layout passes.  Returns a
 *          positioned LayoutNode tree.  Table elements are laid out via the
 *          virtual-scroll-aware TableLayout so that only visible rows
 *          materialise LayoutNodes.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

#include <memory>
#include <vector>

#include "html/DOMNode.h"
#include "layout/LayoutNode.h"
#include "layout/TextMeasurer.h"

namespace layout { class TextMeasurer; }

namespace layout {

class TextMeasurer;

class Box {
public:
    Box();
    ~Box();

    std::vector<std::unique_ptr<LayoutNode>> layout(html::DOMNode* root,
                                                     float viewportWidth = 800.0f,
                                                     float viewportHeight = 600.0f,
                                                     float scrollY = 0.0f);

private:
    std::unique_ptr<LayoutNode> buildBox(html::DOMNode* node,
                                         float containingWidth,
                                         float containingHeight,
                                         float viewportTop,
                                         float viewportBottom);
    void assignComputedStyles(html::DOMNode* node);
    void layoutBlock(LayoutNode* box, float containingWidth, float containingHeight);
    void layoutInline(LayoutNode* box, float containingWidth, float containingHeight);
    float measureTextWidth(const std::string& text, float fontSize, const std::string& fontFamily);
    float measureTextHeight(float fontSize, const std::string& fontFamily);
    void collapseMargins(float& topMargin, float& bottomMargin, const LayoutNode* parent);

    TextMeasurer measurer_;
};

} // namespace layout

#endif // LAYOUT_BOX_H
