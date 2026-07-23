/**
 * @file Box.cpp
 * @brief Layout box implementation.
 * @details Walks the DOM tree, creates LayoutNodes for block-level elements,
 *          and assigns positions based on the viewport width.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/Box.h"
#include "layout/LayoutNode.h"
#include "html/DOMNode.h"
#include "css/Cascade.h"

namespace layout {

Box::Box() = default;
Box::~Box() = default;

static LayoutNode* buildLayout(html::DOMNode* domNode, LayoutNode* parent, int x, int y,
                               int width) {
    if (!domNode) return nullptr;

    LayoutNode* layoutNode = new LayoutNode();
    layoutNode->domNode = domNode;
    layoutNode->parent = parent;
    layoutNode->x = x;
    layoutNode->y = y;
    layoutNode->width = width;

    if (domNode->nodeType == html::NodeType::Element) {
        if (domNode->tagName == "head") {
            delete layoutNode;
            return nullptr;
        }

        css::ComputedStyle style = css::Cascade::computeStyle(domNode, domNode->ownerDocument);
        layoutNode->isBlock = (style.display == css::ComputedStyle::Block
                            || style.display == css::ComputedStyle::InlineBlock
                            || style.display == css::ComputedStyle::Flex
                            || style.display == css::ComputedStyle::Grid
                            || style.display == css::ComputedStyle::Table
                            || style.display == css::ComputedStyle::TableRow
                            || style.display == css::ComputedStyle::TableCell);

        int childY = y;
        int childX = x;
        int childWidth = width;
        int childMarginTop = 0;
        int childMarginBottom = 0;

        if (domNode->tagName == "body") {
            childX = x;
            childWidth = width;
        }

        if (layoutNode->isBlock) {
            childMarginTop = static_cast<int>(style.marginTop);
            childMarginBottom = static_cast<int>(style.marginBottom);
            childY += childMarginTop;
        }

        for (html::DOMNode* child = domNode->firstChild; child; child = child->nextSibling) {
            LayoutNode* childLayout = buildLayout(child, layoutNode, childX, childY, childWidth);
            if (childLayout) {
                childLayout->prevSibling = layoutNode->lastChild;
                if (layoutNode->lastChild) {
                    layoutNode->lastChild->nextSibling = childLayout;
                } else {
                    layoutNode->firstChild = childLayout;
                }
                layoutNode->lastChild = childLayout;
                childY = childLayout->y + childLayout->height;
            }
        }

        if (layoutNode->lastChild) {
            layoutNode->height = layoutNode->lastChild->y + layoutNode->lastChild->height - y + childMarginBottom;
        } else {
            layoutNode->height = 0;
        }
    } else if (domNode->nodeType == html::NodeType::Text) {
        layoutNode->isBlock = false;
        std::string text = domNode->textContent;
        int textWidth = width > 0 ? width : 800;
        int lineHeight = 16;
        if (domNode->parent) {
            css::ComputedStyle parentStyle = css::Cascade::computeStyle(domNode->parent, domNode->ownerDocument);
            lineHeight = static_cast<int>(parentStyle.fontSize * parentStyle.lineHeight);
        }
        int approximateLines = text.empty() ? 0 : ((static_cast<int>(text.length()) * lineHeight / 2) / textWidth) + 1;
        if (approximateLines < 1) approximateLines = 1;
        layoutNode->height = approximateLines * lineHeight + 4;
    } else {
        layoutNode->isBlock = false;
        layoutNode->height = 0;
    }

    return layoutNode;
}

LayoutNode* Box::layout(html::DOMNode* root) {
    if (!root) return nullptr;

    html::DOMNode* htmlNode = nullptr;
    for (html::DOMNode* child = root->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "html") {
            htmlNode = child;
            break;
        }
    }
    if (!htmlNode) return nullptr;

    int viewportWidth = root->style ? root->style->width : 800;
    if (viewportWidth <= 0) viewportWidth = 800;

    return buildLayout(htmlNode, nullptr, 0, 0, viewportWidth);
}

} // namespace layout
