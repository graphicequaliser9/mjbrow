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

namespace layout {

Box::Box() = default;
Box::~Box() = default;

static LayoutNode* buildLayout(html::DOMNode* domNode, LayoutNode* parent, int x, int y,
                               int width) {
    if (!domNode) return nullptr;

    LayoutNode* layoutNode = new LayoutNode();
    layoutNode->parent = parent;
    layoutNode->x = x;
    layoutNode->y = y;
    layoutNode->width = width;

    if (domNode->nodeType == html::NodeType::Element) {
        if (domNode->tagName == "head") {
            delete layoutNode;
            return nullptr;
        }

        layoutNode->isBlock = true;

        int childY = y;
        int childX = x;
        int childWidth = width;

        if (domNode->tagName == "body") {
            childX = x;
            childWidth = width;
        }

        for (html::DOMNode* child = domNode->firstChild; child; child = child->nextSibling) {
            LayoutNode* childLayout = nullptr;
            if (child->nodeType == html::NodeType::Element) {
                childLayout = buildLayout(child, layoutNode, childX, childY, childWidth);
            } else if (child->nodeType == html::NodeType::Text) {
                childLayout = new LayoutNode();
                childLayout->parent = layoutNode;
                childLayout->x = childX;
                childLayout->y = childY;
                childLayout->width = childWidth;
                childLayout->isBlock = false;
                childLayout->height = 16;
            }

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
            layoutNode->height = layoutNode->lastChild->y + layoutNode->lastChild->height - y;
        } else {
            layoutNode->height = 0;
        }
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
