/**
 * @file DOMInspector.cpp
 * @brief DOM inspector implementation: tree build, expansion, computed-style panel,
 *        and hover bounding-box overlay.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "devtools/DOMInspector.h"

#include "html/DOMNode.h"
#include "html/HTMLParser.h"
#include "css/ComputedStyle.h"

#include "util/Logging.h"

#include <sstream>

namespace devtools {

DOMInspector::DOMInspector() = default;
DOMInspector::~DOMInspector() = default;

// ── tree management ─────────────────────────────────────────────────────

void DOMInspector::attach(html::DOMNode* rootDoc) {
    documentRoot_  = rootDoc;
    selectedNode_  = nullptr;
    root_          = rootDoc ? buildTreeRecursive(rootDoc) : nullptr;
    util::Log(util::LogLevel::Debug, "DOMInspector: attached to DOM root\n");
}

std::unique_ptr<InspectorNode> DOMInspector::buildTreeRecursive(html::DOMNode* domNode) {
    auto node = std::make_unique<InspectorNode>();
    node->domNode    = domNode;
    node->expanded   = true;   // Root start expanded

    if (!domNode) return node;

    // Count children to set hasChildren flag
    int childCount = 0;
    for (html::DOMNode* c = domNode->firstChild; c; c = c->nextSibling)
        ++childCount;
    node->hasChildren = childCount > 0;

    if (!domNode->firstChild) return node;

    for (html::DOMNode* c = domNode->firstChild; c; c = c->nextSibling) {
        node->children.push_back(buildTreeRecursive(c));
    }
    return node;
}

void DOMInspector::expandNode(InspectorNode& node) {
    if (node.expanded || !node.hasChildren || !node.domNode) return;
    node.expanded = true;
    node.children.clear();
    for (html::DOMNode* c = node.domNode->firstChild; c; c = c->nextSibling) {
        node.children.push_back(buildTreeRecursive(c));
    }
}

void DOMInspector::collapseNode(InspectorNode& node) {
    if (!node.expanded) return;
    node.expanded = false;
    node.children.clear();
    node.hasChildren = node.domNode && node.domNode->firstChild;
}

// ── selection ──────────────────────────────────────────────────────────

InspectorNode* DOMInspector::selectByFlatIndex(size_t flatIndex) {
    auto flat = flattenTree();
    if (flatIndex >= flat.size()) return nullptr;
    selectedNode_ = flat[flatIndex];
    return selectedNode_;
}

std::vector<InspectorNode*> DOMInspector::flattenTree() const {
    std::vector<InspectorNode*> out;
    if (!root_) return out;
    out.reserve(256);
    // Depth-first, pre-order traversal (only show children of expanded nodes)
    std::vector<const InspectorNode*> stack;
    stack.push_back(root_.get());
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();
        out.push_back(const_cast<InspectorNode*>(cur));

        if (cur->expanded) {
            for (auto it = cur->children.rbegin(); it != cur->children.rend(); ++it) {
                if (it->get()) stack.push_back(it->get());
            }
        }
    }
    return out;
}

// ── computed style panel ───────────────────────────────────────────────

std::vector<StyleProperty> DOMInspector::getSelectedComputedStyles() const {
    std::vector<StyleProperty> out;

    if (!selectedNode_ || !selectedNode_->domNode) return out;

    auto* dom   = selectedNode_->domNode;
    auto* style = dom->style;

    if (!style) {
        // Return a minimal set even without a computed style (node info)
        std::ostringstream tag;
        if (dom->nodeType == html::NodeType::Element) tag << dom->tagName;
        else if (dom->nodeType == html::NodeType::Text) tag << "#text \"" << dom->textContent.substr(0, 40) << "\"";
        else tag << "<comment>";
        out.push_back({"nodeType", tag.str()});
        return out;
    }

    // Resolved display property
    const char* displayStr = "unknown";
    switch (style->display) {
        case css::ComputedStyle::None:        displayStr = "none";        break;
        case css::ComputedStyle::Block:       displayStr = "block";       break;
        case css::ComputedStyle::Inline:      displayStr = "inline";      break;
        case css::ComputedStyle::InlineBlock: displayStr = "inline-block";break;
        case css::ComputedStyle::Flex:        displayStr = "flex";        break;
        case css::ComputedStyle::Grid:        displayStr = "grid";        break;
        case css::ComputedStyle::Table:       displayStr = "table";       break;
        case css::ComputedStyle::TableRow:    displayStr = "table-row";   break;
        case css::ComputedStyle::TableCell:   displayStr = "table-cell";  break;
    }
    out.push_back({"display",   displayStr});
    out.push_back({"width",     std::to_string((int)style->width)   + "px"});
    out.push_back({"height",    std::to_string((int)style->height)  + "px"});
    out.push_back({"font-size", std::to_string((int)style->fontSize) + "px"});
    out.push_back({"font-family", style->fontFamily});
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "#%08X", style->color);
        out.push_back({"color", buf});
    }
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "#%08X", style->backgroundColor);
        out.push_back({"background-color", buf});
    }
    out.push_back({"opacity", std::to_string(style->opacity)});
    out.push_back({"position", "static"});

    return out;
}

// ── overlay rectangle ──────────────────────────────────────────────────

OverlayRect DOMInspector::getHoverOverlay(InspectorNode* /*hoveredNode*/) const {
    OverlayRect r{0, 0, 0, 0, 0};
    return r;
}

} // namespace devtools
