/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser: tokeniser + tree builder driver, plus DOMNode helpers.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include "html/Tokenizer.h"
#include "html/TreeBuilder.h"

#include <cctype>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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
    // Wrap the fragment in a <body> so the tree builder enters InBody mode and
    // preserves bare text (e.g. innerHTML = "hello") which would otherwise be
    // dropped while still in the Initial/BeforeHtml state.
    parser.parseInto(temp, "<body>" + html + "</body>");

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

// ── query / traversal API (Bead 4) ────────────────────────────────────────────

namespace {

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

// A compound selector: a sequence of simple selectors glued together with no
// combinator (e.g. "div.foo#bar").
struct Compound {
    std::string tag;                       ///< tag name, "" or "*" for universal
    std::string id;                        ///< #id, empty if absent
    std::vector<std::string> classes;      ///< .class tokens
};

// A complex selector: an ordered list of compounds joined by combinators.
struct Complex {
    enum Combinator { Descendant, Child } comb{Descendant};
    Compound comp;
};

// Reads a single compound selector starting at index @p i, advancing @p i past
// it. Stops at whitespace or '>' (which are combinator separators). Returns
// true if at least one simple selector was consumed.
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

// Parses a (basic) CSS selector into an ordered list of compounds. The first
// compound's combinator is always Descendant; subsequent compounds carry the
// combinator that separates them from the previous one.
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

// Matches a complex selector against @p subject, treating @p subject as the
// right-most (subject) compound and walking left along ancestors.
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
        } else {  // descendant
            for (const DOMNode* a = context->parent; a; a = a->parent) {
                if (matchCompound(a, c.comp)) { ok = true; context = a; break; }
            }
        }
        if (!ok) return false;
    }
    return true;
}

} // anonymous namespace

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

DOMNode::~DOMNode() {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    firstChild = lastChild = nullptr;
    delete style;
}

Document::Document() {
    nodeType = NodeType::Document;
    namespaceURI = ns::HTML;
    doctype.clear();
}

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

namespace {

std::string escapeText(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::string escapeAttr(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

void serializeRec(const DOMNode* node, std::string& out) {
    if (!node) return;
    switch (node->nodeType) {
    case NodeType::Text:
        out += escapeText(node->textContent);
        break;
    case NodeType::Comment:
        out += "<!--" + node->textContent + "-->";
        break;
    case NodeType::Element: {
        if (node->tagName == "br") { out += "<br>"; break; }
        if (node->tagName == "img" || node->tagName == "hr" ||
            node->tagName == "input" || node->tagName == "meta" ||
            node->tagName == "link") {
            out += "<" + node->tagName;
            for (auto& kv : node->attributes)
                out += " " + kv.first + "=\"" + escapeAttr(kv.second) + "\"";
            out += ">";
            break;
        }
        out += "<" + node->tagName;
        for (auto& kv : node->attributes)
            out += " " + kv.first + "=\"" + escapeAttr(kv.second) + "\"";
        out += ">";
        for (DOMNode* c = node->firstChild; c; c = c->nextSibling)
            serializeRec(c, out);
        out += "</" + node->tagName + ">";
        break;
    }
    default:
        break;
    }
}

} // anonymous namespace

std::string serializeNode(const DOMNode* node) {
    std::string out;
    if (!node) return out;
    if (node->nodeType == NodeType::Document) {
        if (auto* doc = static_cast<const Document*>(node)) {
            if (!doc->doctype.empty()) out += "<!DOCTYPE " + doc->doctype + ">";
        }
        for (DOMNode* c = node->firstChild; c; c = c->nextSibling)
            serializeRec(c, out);
        return out;
    }
    serializeRec(node, out);
    return out;
}

} // namespace html
