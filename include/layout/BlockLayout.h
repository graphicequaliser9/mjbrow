/**
 * @file BlockLayout.h
 * @brief Block formatting context and margin collapsing.
 * @details Lays out block-level boxes in normal flow, handling vertical margin
 *          collapsing, width/height computation, and containing block sizing.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BLOCKLAYOUT_H
#define LAYOUT_BLOCKLAYOUT_H

#include <memory>
#include <vector>

#include "layout/LayoutNode.h"

namespace layout {

class BlockLayout {
public:
    BlockLayout() = default;
    ~BlockLayout() = default;

    void layout(LayoutNode* root, float containingWidth, float containingHeight);

private:
    void layoutBlockChildren(LayoutNode* parent, float containingWidth, float containingHeight);
    float computeUsedWidth(const LayoutNode* box, float containingWidth);
    float computeUsedHeight(const LayoutNode* box);
    float horizontalMargin(const LayoutNode* box);
    float verticalMargin(const LayoutNode* box);
    float borderBoxWidth(const LayoutNode* box);
    float borderBoxHeight(const LayoutNode* box);
    void collapseAdjacentMargins(float& a, float& b);
    void layoutInlineBlock(LayoutNode* box, float containingWidth, float containingHeight);
};

} // namespace layout

#endif // LAYOUT_BLOCKLAYOUT_H
