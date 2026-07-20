/**
 * @file Box.cpp
 * @brief CSS layout engine implementation.
 * @details Walks the DOM, generates a LayoutNode tree, runs block/inline
 *          layout passes, and returns positioned geometry.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/Box.h"

#include "layout/BlockLayout.h"
#include "layout/InlineLayout.h"
#include "layout/TextMeasurer.h"

#include "html/DOMNode.h"
#include "css/Cascade.h"
#include "css/ComputedStyle.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace layout {

Box::Box() = default;
Box::~Box() = default;

void Box::assignComputedStyles(html::DOMNode* node) {
    if (!node) return;
    if (node->nodeType == html::NodeType::Element && node->style == nullptr) {
        if (node->ownerDocument) {
            node->style = new css::ComputedStyle(css::Cascade::computeStyle(node, node->ownerDocument));
        } else {
            node->style = new css::ComputedStyle();
        }
    }
    for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        assignComputedStyles(child);
    }
}

std::vector<std::unique_ptr<LayoutNode>> Box::layout(html::DOMNode* root) {
    std::vector<std::unique_ptr<LayoutNode>> result;
    if (!root) return result;

    assignComputedStyles(root);

    float viewportWidth = 800.0f;
    float viewportHeight = 600.0f;

    if (root->nodeType == html::NodeType::Document) {
        for (html::DOMNode* child = root->firstChild; child; child = child->nextSibling) {
            if (child->nodeType == html::NodeType::Element && child->tagName == "html") {
                for (html::DOMNode* htmlChild = child->firstChild; htmlChild; htmlChild = htmlChild->nextSibling) {
                    if (htmlChild->nodeType == html::NodeType::Element && htmlChild->tagName == "body") {
                        auto bodyBox = buildBox(htmlChild, viewportWidth, viewportHeight);
                        if (bodyBox) {
                            result.push_back(std::move(bodyBox));
                        }
                    }
                }
            }
        }
    } else {
        auto top = buildBox(root, viewportWidth, viewportHeight);
        if (top) result.push_back(std::move(top));
    }

    return result;
}

std::unique_ptr<LayoutNode> Box::buildBox(html::DOMNode* node, float containingWidth, float containingHeight) {
    if (!node) return nullptr;

    css::ComputedStyle* style = node->style;
    if (!style) return nullptr;

    if (style->display == css::ComputedStyle::None) return nullptr;

    auto box = std::make_unique<LayoutNode>();
    box->domNode = node;
    box->display = style->display;
    box->position = style->position;

    float borderLeft = style->borderLeft;
    float borderRight = style->borderRight;
    float borderTop = style->borderTop;
    float borderBottom = style->borderBottom;
    float paddingLeft = style->paddingLeft;
    float paddingRight = style->paddingRight;
    float paddingTop = style->paddingTop;
    float paddingBottom = style->paddingBottom;
    float marginLeft = style->marginLeft;
    float marginRight = style->marginRight;
    float marginTop = style->marginTop;
    float marginBottom = style->marginBottom;

    if (box->position == css::ComputedStyle::Absolute || box->position == css::ComputedStyle::Fixed) {
        box->x = marginLeft + borderLeft + paddingLeft;
        box->y = marginTop + borderTop + paddingTop;
        box->width = (style->width > 0.0f) ? style->width : containingWidth - marginLeft - marginRight - borderLeft - borderRight - paddingLeft - paddingRight;
        box->height = (style->height > 0.0f) ? style->height : 0.0f;
    } else {
        box->x = 0.0f;
        box->y = 0.0f;
        if (style->width > 0.0f) {
            box->width = style->width + borderLeft + borderRight + paddingLeft + paddingRight;
        } else {
            box->width = containingWidth - marginLeft - marginRight;
        }
        box->height = 0.0f;
    }

    if (box->width < 0.0f) box->width = 0.0f;

    if (style->display == css::ComputedStyle::Inline || style->display == css::ComputedStyle::InlineBlock) {
        TextMeasurer measurer;
        std::string text;
        for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
            if (c->nodeType == html::NodeType::Text) text += c->textContent;
        }
        if (!text.empty()) {
            auto metrics = measurer.measure(text, style->fontSize, style->fontFamily);
            if (box->width < metrics.width) {
                box->width = metrics.width + borderLeft + borderRight + paddingLeft + paddingRight;
            }
            box->height = metrics.height + borderTop + borderBottom + paddingTop + paddingBottom;
        } else {
            box->height = style->fontSize + borderTop + borderBottom + paddingTop + paddingBottom;
        }
    }

    for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            if (!child->textContent.empty()) {
                auto textBox = std::make_unique<LayoutNode>();
                textBox->domNode = child;
                textBox->display = css::ComputedStyle::Inline;
                textBox->position = css::ComputedStyle::Static;
                box->children.push_back(std::move(textBox));
            }
        } else if (child->nodeType == html::NodeType::Element) {
            auto childBox = buildBox(child, box->contentWidth(), containingHeight);
            if (childBox) {
                box->children.push_back(std::move(childBox));
            }
        }
    }

    if (style->display == css::ComputedStyle::Block || style->display == css::ComputedStyle::Table) {
        BlockLayout blockLayout;
        blockLayout.layout(box.get(), box->contentWidth(), containingHeight);
    } else if (style->display == css::ComputedStyle::Inline || style->display == css::ComputedStyle::InlineBlock) {
        if (!box->children.empty()) {
            TextMeasurer measurer;
            InlineLayout inlineLayout(measurer);
            inlineLayout.layout(box.get(), box->contentWidth(), containingHeight);
        }
    }

    return box;
}

float Box::measureTextWidth(const std::string& text, float fontSize, const std::string& fontFamily) {
    TextMeasurer measurer;
    auto metrics = measurer.measure(text, fontSize, fontFamily);
    return metrics.width;
}

float Box::measureTextHeight(float fontSize, const std::string& fontFamily) {
    TextMeasurer measurer;
    auto metrics = measurer.measure("", fontSize, fontFamily);
    return metrics.height;
}

void Box::collapseMargins(float& topMargin, float& bottomMargin, const LayoutNode* parent) {
    if (!parent) return;
    float max = (topMargin > bottomMargin) ? topMargin : bottomMargin;
    topMargin = max;
    bottomMargin = max;
}

} // namespace layout

