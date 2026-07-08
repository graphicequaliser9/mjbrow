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

void DOMNode::unlink(DOMNode* child) {
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
}

DOMNode* DOMNode::appendChild(DOMNode* child) {
    if (!child) return nullptr;

    // Move semantics: detach from any prior parent first.
    if (child->parent) child->parent->unlink(child);

    child->parent = this;
    child->prevSibling = lastChild;
    child->nextSibling = nullptr;

    if (lastChild) {
        lastChild->nextSibling = child;
    } else {
        firstChild = child;
    }
    lastChild = child;
    // A Document owns itself; everything else inherits ownerDocument.
    child->ownerDocument = (nodeType == NodeType::Document)
        ? static_cast<Document*>(this)
        : ownerDocument;

    return child;
}

DOMNode* DOMNode::insertBefore(DOMNode* node, DOMNode* child) {
    if (!node) return nullptr;

    // Null/foreign reference child -> append at the end.
    if (!child || child->parent != this) return appendChild(node);

    // Move semantics: detach from any prior parent first.
    if (node->parent) node->parent->unlink(node);

    node->parent = this;
    node->ownerDocument = (nodeType == NodeType::Document)
        ? static_cast<Document*>(this)
        : ownerDocument;
    node->nextSibling = child;
    node->prevSibling = child->prevSibling;

    if (child->prevSibling) {
        child->prevSibling->nextSibling = node;
    } else {
        firstChild = node;
    }
    child->prevSibling = node;

    return node;
}

void DOMNode::removeChild(DOMNode* child) {
    if (!child || child->parent != this) return;
    unlink(child);
    delete child;
}

DOMNode* DOMNode::replaceChild(DOMNode* node, DOMNode* child) {
    if (!node || !child || child->parent != this) return nullptr;
    if (node == child) return node;   // replacing with itself is a no-op

    // Move semantics: detach from any prior parent first.
    if (node->parent) node->parent->unlink(node);

    node->parent = this;
    node->ownerDocument = (nodeType == NodeType::Document)
        ? static_cast<Document*>(this)
        : ownerDocument;
    node->prevSibling = child->prevSibling;
    node->nextSibling = child->nextSibling;

    if (child->prevSibling) {
        child->prevSibling->nextSibling = node;
    } else {
        firstChild = node;
    }
    if (child->nextSibling) {
        child->nextSibling->prevSibling = node;
    } else {
        lastChild = node;
    }

    child->parent = nullptr;
    child->prevSibling = nullptr;
    child->nextSibling = nullptr;
    delete child;

    return node;
}

DOMNode* DOMNode::cloneNode(bool deep) const {
    DOMNode* copy = (nodeType == NodeType::Document)
        ? static_cast<DOMNode*>(new Document())
        : new DOMNode();

    copy->nodeType = nodeType;
    copy->tagName = tagName;
    copy->namespaceURI = namespaceURI;
    copy->textContent = textContent;
    copy->attributes = attributes;

    if (nodeType == NodeType::Document) {
        static_cast<Document*>(copy)->doctype =
            static_cast<const Document*>(this)->doctype;
    }

    if (nodeType == NodeType::Document) {
        // The clone becomes its own owner document for its subtree.
        for (const DOMNode* c = firstChild; c; c = c->nextSibling) {
            DOMNode* cc = c->cloneNode(true);
            cc->parent = copy;
            cc->ownerDocument = static_cast<Document*>(copy);
            cc->prevSibling = copy->lastChild;
            cc->nextSibling = nullptr;
            if (copy->lastChild) copy->lastChild->nextSibling = cc;
            else copy->firstChild = cc;
            copy->lastChild = cc;
        }
    } else if (deep) {
        copy->ownerDocument = ownerDocument;
        for (const DOMNode* c = firstChild; c; c = c->nextSibling) {
            DOMNode* cc = c->cloneNode(true);
            cc->parent = copy;
            cc->ownerDocument = ownerDocument;
            cc->prevSibling = copy->lastChild;
            cc->nextSibling = nullptr;
            if (copy->lastChild) copy->lastChild->nextSibling = cc;
            else copy->firstChild = cc;
            copy->lastChild = cc;
        }
    }

    return copy;
}

void DOMNode::setAttribute(const std::string& name, const std::string& value) {
    attributes[name] = value;
}

const std::string* DOMNode::getAttribute(const std::string& name) const {
    auto it = attributes.find(name);
    return (it == attributes.end()) ? nullptr : &it->second;
}

bool DOMNode::hasAttribute(const std::string& name) const {
    return attributes.find(name) != attributes.end();
}

void DOMNode::removeAttribute(const std::string& name) {
    attributes.erase(name);
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
    doctype.clear();
}

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

} // namespace html
