/**
 * @file InlineLayout.cpp
 * @brief Inline formatting context and line box construction.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/InlineLayout.h"

#include "html/DOMNode.h"
#include "css/ComputedStyle.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace layout {

InlineLayout::InlineLayout(TextMeasurer& measurer) : measurer_(measurer) {}

void InlineLayout::layout(LayoutNode* root, float containingWidth, float containingHeight) {
    if (!root) return;
    std::vector<LineBox> lines;
    buildLineBoxes(root, containingWidth, lines);
    positionLineBoxes(root, lines, root->y);
}

void InlineLayout::buildLineBoxesForChildren(LayoutNode* parent, const std::vector<LayoutNode*>& children,
                                             float containingWidth, std::vector<LineBox>& lines) {
    if (children.empty()) return;

    LineBox currentLine;
    currentLine.y = parent->contentY();
    currentLine.width = 0.0f;
    currentLine.height = 0.0f;

    for (LayoutNode* child : children) {
        if (!child || !child->domNode) continue;

        std::string text;
        if (child->domNode->nodeType == html::NodeType::Text) {
            text = child->domNode->textContent;
        } else {
            text = "<" + child->domNode->tagName + ">";
        }
        if (text.empty()) continue;

        float fontSize = 16.0f;
        std::string fontFamily = "Arial";
        if (child->domNode->parent && child->domNode->parent->style) {
            fontSize = child->domNode->parent->style->fontSize;
            fontFamily = child->domNode->parent->style->fontFamily;
        }
        if (child->domNode->style) {
            fontSize = child->domNode->style->fontSize;
            fontFamily = child->domNode->style->fontFamily;
        }

        auto metrics = measurer_.measure(text, fontSize, fontFamily);

        if (currentLine.width + metrics.width > containingWidth && !currentLine.fragments.empty()) {
            lines.push_back(currentLine);
            currentLine = LineBox();
            currentLine.y = parent->contentY();
            currentLine.width = 0.0f;
            currentLine.height = 0.0f;
        }

        InlineFragment frag;
        frag.domNode = child->domNode;
        frag.text = text;
        frag.width = metrics.width;
        frag.height = metrics.height;
        frag.ascent = metrics.ascent;
        frag.descent = metrics.descent;
        currentLine.fragments.push_back(frag);
        currentLine.width += metrics.width;
        if (metrics.height > currentLine.height) {
            currentLine.height = metrics.height;
        }
    }

    if (!currentLine.fragments.empty()) {
        lines.push_back(currentLine);
    }
}

void InlineLayout::positionInlineChildren(LayoutNode* parent, const std::vector<LayoutNode*>& children,
                                          const std::vector<LineBox>& lines, float startY) {
    if (!parent) return;
    float cursorY = startY;

    size_t fragIdx = 0;
    for (const auto& line : lines) {
        float lineX = parent->contentX();
        for (const auto& frag : line.fragments) {
            if (fragIdx < children.size()) {
                LayoutNode* child = children[fragIdx];
                child->x = lineX;
                child->y = cursorY;
                child->width = frag.width;
                child->height = frag.height;
                lineX += frag.width;
            }
            ++fragIdx;
        }
        cursorY += line.height;
    }
}

void InlineLayout::buildLineBoxes(LayoutNode* root, float containingWidth, std::vector<LineBox>& lines) {
    if (!root || root->children.empty()) return;

    LineBox currentLine;
    currentLine.y = root->y;
    currentLine.width = 0.0f;
    currentLine.height = 0.0f;

    for (const auto& child : root->children) {
        if (!child || !child->domNode) continue;
        if (child->display == css::ComputedStyle::None) continue;

        std::string text;
        if (child->domNode->nodeType == html::NodeType::Text) {
            text = child->domNode->textContent;
        } else {
            text = "<" + child->domNode->tagName + ">";
        }
        if (text.empty()) continue;

        float fontSize = 16.0f;
        std::string fontFamily = "Arial";
        if (child->domNode->style) {
            fontSize = child->domNode->style->fontSize;
            fontFamily = child->domNode->style->fontFamily;
        }

        auto metrics = measurer_.measure(text, fontSize, fontFamily);

        if (currentLine.width + metrics.width > containingWidth && !currentLine.fragments.empty()) {
            lines.push_back(currentLine);
            currentLine = LineBox();
            currentLine.y = root->y;
            currentLine.width = 0.0f;
            currentLine.height = 0.0f;
        }

        InlineFragment frag;
        frag.domNode = child->domNode;
        frag.text = text;
        frag.width = metrics.width;
        frag.height = metrics.height;
        frag.ascent = metrics.ascent;
        frag.descent = metrics.descent;
        currentLine.fragments.push_back(frag);
        currentLine.width += metrics.width;
        if (metrics.height > currentLine.height) {
            currentLine.height = metrics.height;
        }
    }

    if (!currentLine.fragments.empty()) {
        lines.push_back(currentLine);
    }
}

void InlineLayout::positionLineBoxes(LayoutNode* root, const std::vector<LineBox>& lines, float startY) {
    if (!root) return;
    float cursorY = startY;
    float maxWidth = 0.0f;

    size_t childIdx = 0;
    for (const auto& line : lines) {
        float lineX = root->contentX();
        for (const auto& frag : line.fragments) {
            if (childIdx < root->children.size()) {
                auto* child = root->children[childIdx].get();
                child->x = lineX;
                child->y = cursorY;
                child->width = frag.width;
                child->height = frag.height;
                lineX += frag.width;
            }
            ++childIdx;
        }
        if (line.width > maxWidth) maxWidth = line.width;
        cursorY += line.height;
    }

    float borderLeft = root->domNode ? root->domNode->style->borderLeft : 0.0f;
    float borderRight = root->domNode ? root->domNode->style->borderRight : 0.0f;
    float paddingLeft = root->domNode ? root->domNode->style->paddingLeft : 0.0f;
    float paddingRight = root->domNode ? root->domNode->style->paddingRight : 0.0f;
    root->width = maxWidth + borderLeft + borderRight + paddingLeft + paddingRight;
    root->height = (cursorY - startY) + (root->domNode ? root->domNode->style->borderTop + root->domNode->style->borderBottom + root->domNode->style->paddingTop + root->domNode->style->paddingBottom : 0.0f);
}

float InlineLayout::lineSpacing(const LayoutNode* node) const {
    if (!node || !node->domNode || !node->domNode->style) return 0.0f;
    float fontSize = node->domNode->style->fontSize;
    float lineHeight = node->domNode->style->lineHeight;
    return measurer_.lineHeight(fontSize, lineHeight);
}

} // namespace layout
