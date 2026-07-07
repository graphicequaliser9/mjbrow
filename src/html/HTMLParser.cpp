/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser: tokeniser + tree builder driver, plus DOMNode helpers.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include "html/Tokenizer.h"
#include "html/TreeBuilder.h"

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace html {

Document* HTMLParser::parse(const std::string& html) {
    auto* doc = new Document();
    Tokenizer tokenizer(html);
    std::vector<Token> tokens = tokenizer.tokenize();
    TreeBuilder builder(*doc);
    builder.build(tokens);
    return doc;
}

void HTMLParser::parseInto(Document& doc, const std::string& html) {
    // Clear any existing children (without recursing into their subtrees twice).
    while (doc.firstChild) {
        DOMNode* next = doc.firstChild->nextSibling;
        delete doc.firstChild;
        doc.firstChild = next;
    }
    doc.lastChild = nullptr;

    Tokenizer tokenizer(html);
    std::vector<Token> tokens = tokenizer.tokenize();
    TreeBuilder builder(doc);
    builder.build(tokens);
}

void DOMNode::setInnerHTML(const std::string& html) {
    // Detach and delete any existing children.
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    lastChild = nullptr;

    // Parse into a throwaway document, then transfer the <body> content (the
    // fragment is implicitly wrapped in <html>/<body> by the tree builder).
    Document temp;
    HTMLParser parser;
    parser.parseInto(temp, html);

    // Find the content root: prefer the <body> element (which may be nested
    // under <html>), otherwise fall back to the document root.
    DOMNode* contentRoot = &temp;
    {
        std::function<DOMNode*(DOMNode*)> findBody =
            [&](DOMNode* n) -> DOMNode* {
                if (!n) return nullptr;
                if (n->nodeType == NodeType::Element && n->tagName == "body") return n;
                for (DOMNode* c = n->firstChild; c; c = c->nextSibling) {
                    if (DOMNode* f = findBody(c)) return f;
                }
                return nullptr;
            };
        if (DOMNode* body = findBody(&temp)) contentRoot = body;
    }

    DOMNode* child = contentRoot->firstChild;
    while (child) {
        DOMNode* next = child->nextSibling;

        child->parent = this;
        child->prevSibling = lastChild;
        child->nextSibling = nullptr;
        child->ownerDocument = ownerDocument ? ownerDocument : &temp;

        if (lastChild) lastChild->nextSibling = child;
        else firstChild = child;
        lastChild = child;

        child = next;
    }
    // The transferred children are now owned by `this`; detach them from the
    // throwaway document's <body> so its destructor does not delete them.  The
    // remaining document scaffolding (<html>/<head>/<body>) is still owned by
    // `temp` and is released when it goes out of scope.
    contentRoot->firstChild = contentRoot->lastChild = nullptr;
}

DOMNode* DOMNode::appendChild(DOMNode* child) {
    if (!child) return nullptr;

    child->parent = this;
    child->prevSibling = lastChild;
    child->nextSibling = nullptr;

    if (lastChild) {
        lastChild->nextSibling = child;
    } else {
        firstChild = child;
    }
    lastChild = child;
    child->ownerDocument = ownerDocument;

    return child;
}

void DOMNode::removeChild(DOMNode* child) {
    if (!child || child->parent != this) return;

    if (child->prevSibling) {
        child->prevSibling->nextSibling = child->nextSibling;
    } else {
        firstChild = child->nextSibling;
    }

    if (child->nextSibling) {
        child->nextSibling->prevSibling = child->prevSibling;
    } else {
        lastChild = child->prevSibling;
    }

    child->parent = nullptr;
    child->prevSibling = nullptr;
    child->nextSibling = nullptr;
    delete child;
}

DOMNode::~DOMNode() {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    firstChild = lastChild = nullptr;
}

Document::Document() {
    nodeType = NodeType::Document;
    namespaceURI = ns::HTML;
}

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

} // namespace html
