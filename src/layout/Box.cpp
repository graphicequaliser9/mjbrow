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
    layoutNode->domNode = domNode;
    layoutNode->x = x;
    layoutNode->y = y;
    layoutNode->width = width;
    if (parent) {
        layoutNode->parent = parent;
    }

    if (domNode->nodeType == html::NodeType::Element) {
        if (domNode->tagName == "head") {
            delete layoutNode;
            return nullptr;
        }

        layoutNode->isBlock = true;

        int childX = x;
        int childWidth = width;

        if (domNode->style) {
            childX += static_cast<int>(domNode->style->paddingLeft + domNode->style->borderLeft);
            childWidth -= static_cast<int>(domNode->style->paddingLeft + domNode->style->paddingRight
                                         + domNode->style->borderLeft  + domNode->style->borderRight);
        }
        if (childWidth < 0) childWidth = 0;

        int childY = y;
        for (html::DOMNode* child = domNode->firstChild; child; child = child->nextSibling) {
            LayoutNode* childLayout = nullptr;

            if (child->nodeType == html::NodeType::Element) {
                if (child->tagName == "head") continue;

                float marginTop = 0.0f;
                if (child->style) marginTop = child->style->marginTop;
                childY += static_cast<int>(marginTop);

                childLayout = buildLayout(child, layoutNode, childX, childY, childWidth);
                if (!childLayout) continue;

                childY = childLayout->y + childLayout->height;
            } else if (child->nodeType == html::NodeType::Text) {
                childLayout = new LayoutNode();
                childLayout->parent     = layoutNode;
                childLayout->domNode    = child;
                childLayout->x          = childX;
                childLayout->y          = childY;
                childLayout->width      = childWidth;
                childLayout->isBlock    = false;

                std::string text = child->textContent;
                bool onlyWs = true;
                for (char ch : text)
                    if (!std::isspace(static_cast<unsigned char>(ch))) { onlyWs = false; break; }

                if (!onlyWs) {
                    float fontSize   = 16.0f;
                    float lineHeight  = 1.2f;
                    if (domNode && domNode->style) {
                        fontSize   = domNode->style->fontSize;
                        lineHeight  = domNode->style->lineHeight;
                    }
                    float avgCharWidth = fontSize * 0.6f;
                    float availWidth   = static_cast<float>(childWidth);
                    size_t lines = 1;
                    if (availWidth > 0.0f && avgCharWidth > 0.0f) {
                        size_t cpl = static_cast<size_t>(availWidth / avgCharWidth);
                        if (cpl < 1) cpl = 1;
                        lines = (text.size() + cpl - 1) / cpl;
                        if (lines < 1) lines = 1;
                    }
                    childLayout->height = static_cast<int>(lines * fontSize * lineHeight);
                }

                childY = childLayout->y + childLayout->height;
            }

            if (childLayout) {
                childLayout->prevSibling = layoutNode->lastChild;
                if (layoutNode->lastChild) {
                    layoutNode->lastChild->nextSibling = childLayout;
                } else {
                    layoutNode->firstChild = childLayout;
                }
                layoutNode->lastChild = childLayout;
            }
        }

        float marginBottom = 0.0f;
        if (layoutNode->lastChild && layoutNode->lastChild->domNode && layoutNode->lastChild->domNode->style) {
            marginBottom = layoutNode->lastChild->domNode->style->marginBottom;
        }
        if (layoutNode->lastChild) {
            layoutNode->height = layoutNode->lastChild->y + layoutNode->lastChild->height - y + static_cast<int>(marginBottom);
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
