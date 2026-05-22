/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser implementation stub.
 * @details Real implementation in bead 3.  This stub keeps CMakeLists.txt happy.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"

#include "html/DOMNode.h"

namespace html {

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

DOMNode* HTMLParser::parse(const std::string& /*html*/) {
    return nullptr; // placeholder – real DOM built in bead 3
}

void DOMNode::setInnerHTML(const std::string& /*html*/) {
    // placeholder – real innerHTML in bead 3
}

DOMNode* DOMNode::appendChild(DOMNode* /*child*/) {
    return nullptr; // placeholder – real child management in bead 3
}

void DOMNode::removeChild(DOMNode* /*child*/) {
    // placeholder – real child management in bead 3
}

DOMNode::~DOMNode() = default;

Document::Document() {
    nodeType = NodeType::Document;
}

} // namespace html