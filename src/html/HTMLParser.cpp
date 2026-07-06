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
                tokens.push_back({TokenType::EndTag, tagName, {}});
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
                    tokens.push_back({TokenType::Comment, comment, {}});
                } else {
                    while (pos < html.size() && html[pos] != '>') ++pos;
                    if (pos < html.size()) ++pos;
                    tokens.push_back({TokenType::DOCTYPE, "", {}});
                }
            } else {
                std::string tagName = extractTagName(html, pos);
                std::vector<std::pair<std::string, std::string>> attrs;
                extractAttributes(html, pos, attrs);

                bool selfClosing = false;
                if (pos < html.size() && html[pos] == '/') {
                    selfClosing = true;
                    ++pos;
                }
                while (pos < html.size() && html[pos] != '>') ++pos;
                if (pos < html.size()) ++pos;

                Token tok{TokenType::StartTag, tagName, attrs};
                tokens.push_back(tok);
            }
        } else {
            size_t start = pos;
            while (pos < html.size() && html[pos] != '<') ++pos;
            if (pos > start) {
                tokens.push_back({TokenType::Character, html.substr(start, pos - start), {}});
            }
        }
    }
    tokens.push_back({TokenType::EOF_TOKEN, "", {}});
    return tokens;
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

    std::vector<DOMNode*> openElements;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& tok = tokens[i];

        if (tok.type == TokenType::StartTag) {
            auto* element = new DOMNode();
            element->nodeType = NodeType::Element;
            element->tagName = tok.data;
            element->ownerDocument = &doc;
            for (const auto& attr : tok.attributes) {
                element->attributes[attr.first] = attr.second;
            }

            if (!openElements.empty()) {
                DOMNode* parent = openElements.back();
                element->parent = parent;
                if (parent->lastChild) {
                    parent->lastChild->nextSibling = element;
                    element->prevSibling = parent->lastChild;
                } else {
                    parent->firstChild = element;
                }
                parent->lastChild = element;
            } else {
                element->parent = &doc;
                if (doc.lastChild) {
                    doc.lastChild->nextSibling = element;
                    element->prevSibling = doc.lastChild;
                } else {
                    doc.firstChild = element;
                }
                doc.lastChild = element;
            }

            if (!isVoidElement(tok.data)) {
                openElements.push_back(element);
            }

        } else if (tok.type == TokenType::EndTag) {
            std::string tagName = tok.data;
            for (auto it = openElements.rbegin(); it != openElements.rend(); ++it) {
                if ((*it)->tagName == tagName) {
                    openElements.erase(std::next(it).base(), openElements.end());
                    break;
                }
            }

        } else if (tok.type == TokenType::Character) {
            if (tok.data.empty()) continue;

            auto* textNode = new DOMNode();
            textNode->nodeType = NodeType::Text;
            textNode->textContent = tok.data;
            textNode->ownerDocument = &doc;

            if (!openElements.empty()) {
                DOMNode* parent = openElements.back();
                textNode->parent = parent;
                if (parent->lastChild) {
                    parent->lastChild->nextSibling = textNode;
                    textNode->prevSibling = parent->lastChild;
                } else {
                    parent->firstChild = textNode;
                }
                parent->lastChild = textNode;
            } else {
                textNode->parent = &doc;
                if (doc.lastChild) {
                    doc.lastChild->nextSibling = textNode;
                    textNode->prevSibling = doc.lastChild;
                } else {
                    doc.firstChild = textNode;
                }
                doc.lastChild = textNode;
            }
        }
    }

    return &doc;
}

DOMNode* HTMLParser::parse(const std::string& html) {
    static Document doc;
    doc.firstChild = nullptr;
    doc.lastChild = nullptr;
    doc.nodeType = NodeType::Document;
    
    return parseHTML(html, doc);
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

    DOMNode* child = tempDoc.firstChild;
    while (child) {
        DOMNode* clone = new DOMNode();
        clone->nodeType = child->nodeType;
        clone->tagName = child->tagName;
        clone->textContent = child->textContent;
        clone->attributes = child->attributes;
        clone->parent = this;
        clone->ownerDocument = ownerDocument;

        if (lastChild) {
            lastChild->nextSibling = clone;
            clone->prevSibling = lastChild;
        } else {
            firstChild = clone;
        }
        lastChild = clone;

        child = child->nextSibling;
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