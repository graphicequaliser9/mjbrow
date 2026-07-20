/**
 * @file InlineLayout.h
 * @brief Inline formatting context and line box construction.
 * @details Collects inline-level children (inline elements + text nodes),
 *          measures them, breaks them into line boxes at the containing width,
 *          and positions each line vertically.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_INLINELAYOUT_H
#define LAYOUT_INLINELAYOUT_H

#include <memory>
#include <string>
#include <vector>

#include "layout/LayoutNode.h"
#include "layout/TextMeasurer.h"

namespace layout {

class InlineLayout {
public:
    struct InlineFragment {
        const html::DOMNode* domNode{nullptr};
        std::string text;
        float width{0.0f};
        float height{0.0f};
        float ascent{0.0f};
        float descent{0.0f};
    };

    struct LineBox {
        float y{0.0f};
        float height{0.0f};
        float width{0.0f};
        std::vector<InlineFragment> fragments;
    };

    InlineLayout(TextMeasurer& measurer);
    ~InlineLayout() = default;

    void layout(LayoutNode* root, float containingWidth, float containingHeight);
    void buildLineBoxesForChildren(LayoutNode* parent, const std::vector<LayoutNode*>& children,
                                   float containingWidth, std::vector<LineBox>& lines);
    void positionInlineChildren(LayoutNode* parent, const std::vector<LayoutNode*>& children,
                                const std::vector<LineBox>& lines, float startY);

private:
    void buildLineBoxes(LayoutNode* root, float containingWidth, std::vector<LineBox>& lines);
    void positionLineBoxes(LayoutNode* root, const std::vector<LineBox>& lines, float startY);
    float lineSpacing(const LayoutNode* node) const;

    TextMeasurer& measurer_;
};

} // namespace layout

#endif // LAYOUT_INLINELAYOUT_H
