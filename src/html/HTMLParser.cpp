/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser implementation stub.
 * @details Real implementation in bead 3.  This stub keeps CMakeLists.txt happy.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"

#include "html/DOMNode.h"

namespace html {

#include "html/HTMLParser.h"
#include "html/DOMNode.h"

#include <memory>

namespace html {

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

class Document; // forward declare

// Internal storage for parsed document
static std::unique_ptr<Document> g_documentStorage;

DOMNode* HTMLParser::parse(const std::string& html) {
    // Minimal parser: extract body content for basic rendering
    auto doc = std::make_unique<Document>();
    
    // Find body tag
    size_t bodyStart = html.find("<body");
    size_t bodyEnd = html.find("</body>");
    
    if (bodyStart != std::string::npos) {
        size_t bodyContentStart = html.find('>', bodyStart);
        if (bodyContentStart != std::string::npos && bodyEnd != std::string::npos) {
            std::string bodyContent = html.substr(bodyContentStart + 1, bodyEnd - bodyContentStart - 1);
            // Strip tags for simple text rendering
            std::string text;
            bool inTag = false;
            for (char c : bodyContent) {
                if (c == '<') inTag = true;
                else if (c == '>') inTag = false;
                else if (!inTag) text += c;
            }
            // Store as body text node
            if (auto* bodyNode = new DOMNode()) {
                bodyNode->nodeType = NodeType::Element;
                bodyNode->tagName = "body";
                bodyNode->textContent = text;
                doc->firstChild = bodyNode;
                bodyNode->parent = doc.get();
            }
        }
    }
    
    g_documentStorage = std::move(doc);
    return g_documentStorage.get();
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