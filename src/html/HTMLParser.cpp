/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"

#include <memory>
#include <vector>
#include <string>
#include <cctype>

namespace html {

static const std::vector<std::string> voidElements = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"
};

static bool isVoidElement(const std::string& tag) {
    std::string lowerTag;
    lowerTag.reserve(tag.size());
    for (char c : tag) lowerTag += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const auto& voidTag : voidElements) {
        if (lowerTag == voidTag) return true;
    }
    return false;
}

static const std::vector<std::string> mathElementNames = {
    "math"
};

static const std::vector<std::string> svgElementNames = {
    "svg"
};

static const std::vector<std::string> tableElements = {
    "table", "tbody", "thead", "tfoot", "tr", "td", "th", "caption", "colgroup", "col"
};

static bool isMathMLElement(const std::string& tag) {
    for (const auto& t : mathElementNames) if (tag == t) return true;
    return false;
}

static bool isSVGElement(const std::string& tag) {
    for (const auto& t : svgElementNames) if (tag == t) return true;
    return false;
}

static bool isTableElement(const std::string& tag) {
    for (const auto& t : tableElements) if (tag == t) return true;
    return false;
}

static void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) { ++pos; }
}

static std::string toLower(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

static std::string extractTagName(const std::string& s, size_t& pos) {
    size_t start = pos;
    while (pos < s.size() && !std::isspace(static_cast<unsigned char>(s[pos])) && s[pos] != '>' && s[pos] != '/') {
        ++pos;
    }
    return toLower(s.substr(start, pos - start));
}

static void extractAttributes(const std::string& s, size_t& pos, std::vector<std::pair<std::string, std::string>>& attrs) {
    while (pos < s.size()) {
        skipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] == '>' || s[pos] == '/') break;

        size_t nameStart = pos;
        while (pos < s.size() && !std::isspace(static_cast<unsigned char>(s[pos])) && s[pos] != '=' && s[pos] != '>') {
            ++pos;
        }
        std::string name = toLower(s.substr(nameStart, pos - nameStart));

        skipWhitespace(s, pos);
        std::string value;
        if (pos < s.size() && s[pos] == '=') {
            ++pos;
            skipWhitespace(s, pos);
            if (pos < s.size() && (s[pos] == '"' || s[pos] == '\'')) {
                char quote = s[pos++];
                size_t valStart = pos;
                while (pos < s.size() && s[pos] != quote) ++pos;
                value = s.substr(valStart, pos - valStart);
                if (pos < s.size()) ++pos;
            } else {
                size_t valStart = pos;
                while (pos < s.size() && !std::isspace(static_cast<unsigned char>(s[pos])) && s[pos] != '>') ++pos;
                value = s.substr(valStart, pos - valStart);
            }
        }
        attrs.emplace_back(name, value);
    }
}

static Token makeToken(TokenType type, std::string data = {},
                        std::vector<std::pair<std::string, std::string>> attrs = {},
                        bool forceQuirks = false, std::string publicId = {},
                        std::string systemId = {}) {
    Token t;
    t.type = type;
    t.data = std::move(data);
    t.attributes = std::move(attrs);
    t.forceQuirks = forceQuirks;
    t.publicId = std::move(publicId);
    t.systemId = std::move(systemId);
    return t;
}

static std::vector<Token> tokenize(const std::string& html) {
    std::vector<Token> tokens;
    size_t pos = 0;

    while (pos < html.size()) {
        if (html[pos] == '<') {
            ++pos;
            if (pos >= html.size()) break;

            if (html[pos] == '/') {
                ++pos;
                skipWhitespace(html, pos);
                std::string tagName = extractTagName(html, pos);
                tokens.push_back(makeToken(TokenType::EndTag, tagName));
                while (pos < html.size() && html[pos] != '>') ++pos;
                if (pos < html.size()) ++pos;
            } else if (html[pos] == '!') {
                ++pos;
                if (pos < html.size() && html[pos] == '-' && pos + 1 < html.size() && html[pos + 1] == '-') {
                    pos += 2;
                    size_t start = pos;
                    while (pos + 2 < html.size() && !(html[pos] == '-' && html[pos + 1] == '-' && html[pos + 2] == '>')) ++pos;
                    std::string comment = html.substr(start, pos - start);
                    if (pos + 2 < html.size()) pos += 3;
                    tokens.push_back(makeToken(TokenType::Comment, comment));
                } else {
                    // DOCTYPE: <!DOCTYPE html ...>
                    Token doctype = makeToken(TokenType::DOCTYPE, "html");
                    skipWhitespace(html, pos);
                    // consume the "doctype" keyword
                    while (pos < html.size() && std::isalpha(static_cast<unsigned char>(html[pos]))) ++pos;
                    skipWhitespace(html, pos);

                    auto readQuotedOrWord = [&](std::string& out) {
                        if (pos < html.size() && (html[pos] == '"' || html[pos] == '\'')) {
                            char q = html[pos++];
                            size_t st = pos;
                            while (pos < html.size() && html[pos] != q) ++pos;
                            out = html.substr(st, pos - st);
                            if (pos < html.size()) ++pos;
                        } else {
                            size_t st = pos;
                            while (pos < html.size() && !std::isspace(static_cast<unsigned char>(html[pos])) && html[pos] != '>')
                                ++pos;
                            out = html.substr(st, pos - st);
                        }
                    };

                    if (pos < html.size() && html[pos] != '>') {
                        readQuotedOrWord(doctype.data);
                        skipWhitespace(html, pos);
                        if (pos + 5 < html.size() && html.compare(pos, 6, "public") == 0) {
                            pos += 6;
                            skipWhitespace(html, pos);
                            readQuotedOrWord(doctype.publicId);
                            skipWhitespace(html, pos);
                            if (pos < html.size() && (html[pos] == '"' || html[pos] == '\'')) {
                                readQuotedOrWord(doctype.systemId);
                            }
                        } else if (pos + 5 < html.size() && html.compare(pos, 6, "system") == 0) {
                            pos += 6;
                            skipWhitespace(html, pos);
                            readQuotedOrWord(doctype.systemId);
                        }
                    }

                    while (pos < html.size() && html[pos] != '>') ++pos;
                    if (pos < html.size()) ++pos;
                    tokens.push_back(doctype);
                }
            } else {
                std::string tagName = extractTagName(html, pos);
                std::vector<std::pair<std::string, std::string>> attrs;
                extractAttributes(html, pos, attrs);

                if (pos < html.size() && html[pos] == '/') {
                    ++pos;
                }
                while (pos < html.size() && html[pos] != '>') ++pos;
                if (pos < html.size()) ++pos;

                Token tok = makeToken(TokenType::StartTag, tagName, attrs);
                tokens.push_back(tok);
            }
        } else {
            size_t start = pos;
            while (pos < html.size() && html[pos] != '<') ++pos;
            if (pos > start) {
                tokens.push_back(makeToken(TokenType::Character, html.substr(start, pos - start)));
            }
        }
    }
    tokens.push_back(makeToken(TokenType::EOF_TOKEN));
    return tokens;
}

static void appendChildRaw(DOMNode* parent, DOMNode* child) {
    child->parent = parent;
    if (parent->lastChild) {
        parent->lastChild->nextSibling = child;
        child->prevSibling = parent->lastChild;
    } else {
        parent->firstChild = child;
    }
    parent->lastChild = child;
}

static DOMNode* deepClone(DOMNode* node, Document* owner) {
    if (!node) return nullptr;
    auto* clone = new DOMNode();
    clone->nodeType = node->nodeType;
    clone->tagName = node->tagName;
    clone->textContent = node->textContent;
    clone->attributes = node->attributes;
    clone->namespaceURI = node->namespaceURI;
    clone->ownerDocument = owner;
    for (DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        DOMNode* c = deepClone(child, owner);
        c->parent = clone;
        if (clone->lastChild) {
            clone->lastChild->nextSibling = c;
            c->prevSibling = clone->lastChild;
        } else {
            clone->firstChild = c;
        }
        clone->lastChild = c;
    }
    return clone;
}

static DOMNode* parseHTML(const std::string& html, Document& doc) {
    std::vector<Token> tokens = tokenize(html);

    if (doc.firstChild) {
        DOMNode* child = doc.firstChild;
        while (child) {
            DOMNode* next = child->nextSibling;
            delete child;
            child = next;
        }
        doc.firstChild = doc.lastChild = nullptr;
    }
    doc.doctypeName.clear();
    doc.doctypePublicId.clear();
    doc.doctypeSystemId.clear();
    doc.quirksMode = false;

    std::vector<DOMNode*> openElements;

    // Current namespace stack, aligned with openElements (and document root).
    std::vector<const char*> nsStack;

    auto open = [&](const std::string& tag, const std::vector<std::pair<std::string, std::string>>& attrs,
                    const char* ns, bool isVoid) {
        auto* element = new DOMNode();
        element->nodeType = NodeType::Element;
        element->tagName = tag;
        element->namespaceURI = ns;
        element->ownerDocument = &doc;
        for (const auto& attr : attrs) {
            element->attributes[attr.first] = attr.second;
        }

        DOMNode* parent = openElements.empty() ? static_cast<DOMNode*>(&doc) : openElements.back();

        // Simplified foster-parenting: content that belongs inside a table
        // cell/row but appears directly under <table>/<tbody>/<thead>/<tfoot>
        // is adopted up to the most recent <tr>/<td>/<th> (<tr> preferred).
        if (parent != &doc && isTableElement(parent->tagName) && parent->tagName != "table" &&
            !isTableElement(tag) && tag != "caption") {
            DOMNode* foster = nullptr;
            for (auto it = openElements.rbegin(); it != openElements.rend(); ++it) {
                const std::string& t = (*it)->tagName;
                if (t == "tr") { foster = *it; break; }
                if (t == "td" || t == "th") { foster = *it; break; }
            }
            if (foster) parent = foster;
        }

        appendChildRaw(parent, element);

        if (!isVoid) {
            openElements.push_back(element);
            nsStack.push_back(ns);
        }
        return element;
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& tok = tokens[i];

        if (tok.type == TokenType::DOCTYPE) {
            doc.doctypeName = tok.data;
            doc.doctypePublicId = tok.publicId;
            doc.doctypeSystemId = tok.systemId;
            doc.quirksMode = tok.forceQuirks;

        } else if (tok.type == TokenType::Comment) {
            auto* comment = new DOMNode();
            comment->nodeType = NodeType::Comment;
            comment->textContent = tok.data;
            comment->ownerDocument = &doc;
            appendChildRaw(openElements.empty() ? static_cast<DOMNode*>(&doc) : openElements.back(), comment);

        } else if (tok.type == TokenType::StartTag) {
            const char* ns = kNamespaceHTML;
            if (isMathMLElement(tok.data)) ns = kNamespaceMathML;
            else if (isSVGElement(tok.data)) ns = kNamespaceSVG;

            open(tok.data, tok.attributes, ns, isVoidElement(tok.data));

        } else if (tok.type == TokenType::EndTag) {
            std::string tagName = tok.data;
            for (auto it = openElements.rbegin(); it != openElements.rend(); ++it) {
                if ((*it)->tagName == tagName) {
                    // Pop everything from this element (inclusive).
                    size_t count = static_cast<size_t>(std::distance(openElements.rbegin(), it)) + 1;
                    for (size_t k = 0; k < count; ++k) {
                        nsStack.pop_back();
                        openElements.pop_back();
                    }
                    break;
                }
            }

        } else if (tok.type == TokenType::Character) {
            if (tok.data.empty()) continue;

            auto* textNode = new DOMNode();
            textNode->nodeType = NodeType::Text;
            textNode->textContent = tok.data;
            textNode->ownerDocument = &doc;

            DOMNode* parent = openElements.empty() ? static_cast<DOMNode*>(&doc) : openElements.back();

            // Coalesce adjacent text nodes (simplified adoption-agency text fix).
            if (parent->lastChild && parent->lastChild->nodeType == NodeType::Text) {
                parent->lastChild->textContent += tok.data;
                delete textNode;
            } else {
                appendChildRaw(parent, textNode);
            }
        }
    }

    return &doc;
}

DOMNode* HTMLParser::parse(const std::string& html) {
    auto* doc = new Document();

    Document scratch;
    parseHTML(html, scratch);

    // The scratch document owns its children; deep-copy them into the
    // heap-allocated document so the caller owns a self-contained tree.
    for (DOMNode* child = scratch.firstChild; child; child = child->nextSibling) {
        DOMNode* c = deepClone(child, doc);
        c->parent = doc;
        if (doc->lastChild) {
            doc->lastChild->nextSibling = c;
            c->prevSibling = doc->lastChild;
        } else {
            doc->firstChild = c;
        }
        doc->lastChild = c;
    }

    doc->doctypeName = scratch.doctypeName;
    doc->doctypePublicId = scratch.doctypePublicId;
    doc->doctypeSystemId = scratch.doctypeSystemId;
    doc->quirksMode = scratch.quirksMode;

    return doc;
}

void DOMNode::setInnerHTML(const std::string& html) {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        delete firstChild;
        firstChild = next;
    }
    lastChild = firstChild = nullptr;

    Document tempDoc;
    parseHTML(html, tempDoc);

    Document* owner = ownerDocument ? ownerDocument : dynamic_cast<Document*>(this);
    for (DOMNode* child = tempDoc.firstChild; child; child = child->nextSibling) {
        DOMNode* c = deepClone(child, owner);
        c->parent = this;
        if (lastChild) {
            lastChild->nextSibling = c;
            c->prevSibling = lastChild;
        } else {
            firstChild = c;
        }
        lastChild = c;
    }
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
}

HTMLParser::HTMLParser() = default;
HTMLParser::~HTMLParser() = default;

} // namespace html