/**
 * @file html/DOMNode.cpp
 * @brief DOM node implementations: tree manipulation, serialization, and query.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/DOMNode.h"
#include "html/HTMLParser.h"
#include "html/TreeBuilder.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace html {

namespace {

std::string escapeHTML(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '\"': out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += ch;       break;
        }
    }
    return out;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

struct Compound {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
};

struct Complex {
    enum Combinator { Descendant, Child } comb{Descendant};
    Compound comp;
};

bool parseCompound(const std::string& s, size_t& i, Compound& out) {
    out = Compound{};
    const size_t n = s.size();
    bool any = false;
    while (i < n) {
        char ch = s[i];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '>') break;
        if (ch == '#') {
            ++i;
            std::string v;
            while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
                   s[i] != '>' && s[i] != '#' && s[i] != '.')
                v += s[i++];
            out.id = v;
            any = true;
        } else if (ch == '.') {
            ++i;
            std::string v;
            while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
                   s[i] != '>' && s[i] != '#' && s[i] != '.')
                v += s[i++];
            out.classes.push_back(v);
            any = true;
        } else if (ch == '*') {
            out.tag = "*";
            ++i;
            any = true;
        } else {
            std::string v;
            while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) && s[i] != '>' &&
                   s[i] != '#' && s[i] != '.')
                v += s[i++];
            out.tag = v;
            any = true;
        }
    }
    return any;
}

std::vector<Complex> parseSelector(const std::string& sel) {
    std::vector<Complex> out;
    size_t i = 0;
    const size_t n = sel.size();
    while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) ++i;

    Compound first;
    if (!parseCompound(sel, i, first)) return out;
    out.push_back(Complex{Complex::Descendant, first});

    while (i < n) {
        Complex::Combinator comb = Complex::Descendant;
        while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) { comb = Complex::Descendant; ++i; }
        if (i < n && sel[i] == '>') {
            comb = Complex::Child;
            ++i;
            while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) ++i;
        }
        Compound c;
        if (!parseCompound(sel, i, c)) break;
        out.push_back(Complex{comb, c});
    }
    return out;
}

bool matchCompound(const DOMNode* node, const Compound& c) {
    if (node->nodeType != NodeType::Element) return false;
    if (!c.tag.empty() && c.tag != "*" && !iequals(node->tagName, c.tag)) return false;
    if (!c.id.empty()) {
        auto it = node->attributes.find("id");
        if (it == node->attributes.end() || it->second != c.id) return false;
    }
    for (const auto& cls : c.classes) {
        auto it = node->attributes.find("class");
        if (it == node->attributes.end()) return false;
        bool found = false;
        std::istringstream ss(it->second);
        std::string tok;
        while (ss >> tok) {
            if (tok == cls) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

bool matchChain(const DOMNode* subject, const std::vector<Complex>& chain) {
    if (chain.empty()) return false;
    if (!matchCompound(subject, chain.back().comp)) return false;

    const DOMNode* context = subject;
    for (int k = static_cast<int>(chain.size()) - 2; k >= 0; --k) {
        const Complex& c = chain[k];
        bool ok = false;
        if (c.comb == Complex::Child) {
            const DOMNode* p = context->parent;
            if (p && matchCompound(p, c.comp)) { ok = true; context = p; }
        } else {
            for (const DOMNode* a = context->parent; a; a = a->parent) {
                if (matchCompound(a, c.comp)) { ok = true; context = a; break; }
            }
        }
        if (!ok) return false;
    }
    return true;
}

} // anonymous namespace

// ── innerHTML interface ────────────────────────────────────────────────────────

void DOMNode::setInnerHTML(const std::string& html) {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    lastChild = nullptr;

    Document temp;
    HTMLParser parser;
    parser.parseInto(temp, html);

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
    contentRoot->firstChild = contentRoot->lastChild = nullptr;
}

std::string DOMNode::getInnerHTML() const {
    if (nodeType == NodeType::Element) {
        std::string out = "<" + tagName;
        for (const auto& attr : attributes) {
            std::string value = escapeHTML(attr.second);
            out += " " + attr.first + "=\"" + value + "\"";
        }
        if (html::TreeBuilder::isVoidElement(tagName)) {
            out += ">";
        } else {
            out += ">";
            for (const DOMNode* c = firstChild; c; c = c->nextSibling) {
                out += c->getInnerHTML();
            }
            out += "</" + tagName + ">";
        }
        return out;
    }
    if (nodeType == NodeType::Text) {
        return escapeHTML(textContent);
    }
    if (nodeType == NodeType::Comment) {
        return "<!--" + textContent + "-->";
    }
    std::string out;
    for (const DOMNode* c = firstChild; c; c = c->nextSibling) {
        out += c->getInnerHTML();
    }
    return out;
}

// ── tree manipulation ──────────────────────────────────────────────────────────

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
    child->ownerDocument = (nodeType == NodeType::Document)
        ? static_cast<Document*>(this)
        : ownerDocument;

    return child;
}

DOMNode* DOMNode::insertBefore(DOMNode* node, DOMNode* child) {
    if (!node) return nullptr;

    if (!child || child->parent != this) return appendChild(node);

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
    if (node == child) return node;

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

// ── attribute helpers ──────────────────────────────────────────────────────────

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

// ── query / traversal API (Bead 4) ────────────────────────────────────────────

std::string DOMNode::gatherText() const {
    if (nodeType == NodeType::Text) return textContent;
    std::string out;
    for (const DOMNode* c = firstChild; c; c = c->nextSibling) {
        out += c->gatherText();
    }
    return out;
}

DOMNode* DOMNode::getElementById(const std::string& id) {
    if (nodeType == NodeType::Element) {
        auto it = attributes.find("id");
        if (it != attributes.end() && it->second == id) return this;
    }
    for (DOMNode* c = firstChild; c; c = c->nextSibling) {
        if (DOMNode* found = c->getElementById(id)) return found;
    }
    return nullptr;
}

std::vector<DOMNode*> DOMNode::getElementsByTagName(const std::string& tag) {
    std::vector<DOMNode*> out;
    bool all = (tag == "*");
    if (nodeType == NodeType::Element && (all || iequals(tagName, tag))) {
        out.push_back(this);
    }
    for (DOMNode* c = firstChild; c; c = c->nextSibling) {
        auto child = c->getElementsByTagName(tag);
        out.insert(out.end(), child.begin(), child.end());
    }
    return out;
}

DOMNode* DOMNode::querySelector(const std::string& selector) {
    std::vector<Complex> chain = parseSelector(selector);
    if (chain.empty()) return nullptr;

    std::function<DOMNode*(DOMNode*)> walk = [&](DOMNode* n) -> DOMNode* {
        if (!n) return nullptr;
        if (n->nodeType == NodeType::Element && matchChain(n, chain)) return n;
        for (DOMNode* c = n->firstChild; c; c = c->nextSibling) {
            if (DOMNode* found = walk(c)) return found;
        }
        return nullptr;
    };
    return walk(this);
}

std::vector<DOMNode*> DOMNode::querySelectorAll(const std::string& selector) {
    std::vector<Complex> chain = parseSelector(selector);
    std::vector<DOMNode*> out;
    if (chain.empty()) return out;

    std::function<void(DOMNode*)> walk = [&](DOMNode* n) {
        if (!n) return;
        if (n->nodeType == NodeType::Element && matchChain(n, chain)) out.push_back(n);
        for (DOMNode* c = n->firstChild; c; c = c->nextSibling) walk(c);
    };
    walk(this);
    return out;
}

// ── destructor ────────────────────────────────────────────────────────────────

DOMNode::~DOMNode() {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    firstChild = lastChild = nullptr;
}

// ── Document constructor ──────────────────────────────────────────────────────

Document::Document() {
    nodeType = NodeType::Document;
    namespaceURI = ns::HTML;
    doctype.clear();
}

} // namespace html
