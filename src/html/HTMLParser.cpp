/**
 * @file HTMLParser.cpp
 * @brief HTML5 parser implementation with tokenizer and tree builder.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

namespace html {

static bool isVoidElement(const std::string& tagName) {
    const char* voidElements[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    for (const char* elem : voidElements) {
        if (tagName == elem) return true;
    }
    return false;
}

static void toStringLowerCase(std::string& s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
    }
}

class Tokenizer {
public:
    explicit Tokenizer(const std::string& source) : source_(source), pos_(0) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < source_.size()) {
            Token token = nextToken();
            tokens.push_back(token);
            if (token.type == TokenType::EOF_TOKEN) break;
        }
        return tokens;
    }

private:
    std::string source_;
    size_t pos_;

    void skipWhitespace() {
        while (pos_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[pos_]))) {
            pos_++;
        }
    }

    Token nextToken() {
        skipWhitespace();

        if (pos_ >= source_.size()) {
            return {TokenType::EOF_TOKEN, "", {}};
        }

        char c = source_[pos_];

        if (c == '<') {
            if (pos_ + 4 < source_.size() && source_.substr(pos_, 4) == "<!--") {
                return parseComment();
            }
            if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '!') {
                return parseDOCTYPE();
            }
            return parseTag();
        }

        return parseText();
    }

    Token parseComment() {
        size_t start = pos_;
        pos_ += 4; // skip <!--
        size_t end = source_.find("-->", pos_);
        if (end == std::string::npos) {
            Token t{TokenType::Comment, source_.substr(pos_), {}};
            pos_ = source_.size();
            return t;
        }
        Token t{TokenType::Comment, source_.substr(pos_, end - pos_), {}};
        pos_ = end + 3;
        return t;
    }

    Token parseDOCTYPE() {
        pos_ += 2; // skip <!
        size_t end = source_.find('>', pos_);
        if (end == std::string::npos) {
            Token t{TokenType::DOCTYPE, source_.substr(pos_), {}};
            pos_ = source_.size();
            return t;
        }
        Token t{TokenType::DOCTYPE, source_.substr(pos_, end - pos_), {}};
        pos_ = end + 1;
        return t;
    }

    Token parseTag() {
        pos_++; // skip '<'

        if (source_[pos_] == '/') {
            // End tag
            pos_++;
            size_t end = source_.find('>', pos_);
            if (end == std::string::npos) {
                Token t{TokenType::EndTag, "", {}};
                pos_ = source_.size();
                return t;
            }
            Token t{TokenType::EndTag, source_.substr(pos_, end - pos_), {}};
            pos_ = end + 1;
            toStringLowerCase(t.data);
            return t;
        }

        // Start tag
        size_t nameEnd = source_.find_first_of(" />\t\n\r", pos_);
        std::string tagName = source_.substr(pos_, nameEnd - pos_);
        toStringLowerCase(tagName);
        pos_ = nameEnd;

        std::vector<std::pair<std::string, std::string>> attrs;

        while (pos_ < source_.size() && source_[pos_] != '>') {
            skipWhitespace();
            if (pos_ >= source_.size() || source_[pos_] == '>' || source_[pos_] == '/') {
                break;
            }

            size_t attrNameEnd = source_.find_first_of("= />\t\n\r", pos_);
            std::string attrName = source_.substr(pos_, attrNameEnd - pos_);
            pos_ = attrNameEnd;

            skipWhitespace();
            std::string attrValue;
            if (pos_ < source_.size() && source_[pos_] == '=') {
                pos_++; // skip '='
                skipWhitespace();
                if (pos_ < source_.size() && source_[pos_] == '"') {
                    pos_++; // skip opening quote
                    size_t valueEnd = source_.find('"', pos_);
                    attrValue = source_.substr(pos_, valueEnd - pos_);
                    pos_ = valueEnd + 1;
                } else if (pos_ < source_.size() && source_[pos_] == '\'') {
                    pos_++; // skip opening quote
                    size_t valueEnd = source_.find('\'', pos_);
                    attrValue = source_.substr(pos_, valueEnd - pos_);
                    pos_ = valueEnd + 1;
                } else {
                    size_t valueEnd = source_.find_first_of(" />\t\n\r", pos_);
                    attrValue = source_.substr(pos_, valueEnd - pos_);
                    pos_ = valueEnd;
                }
            }
            attrs.push_back({attrName, attrValue});
        }

        // Consume '>' or '/>'
        if (pos_ < source_.size() && source_[pos_] == '/') {
            pos_++; // self-closing slash
        }
        if (pos_ < source_.size() && source_[pos_] == '>') {
            pos_++;
        }

        return {TokenType::StartTag, tagName, attrs};
    }

    Token parseText() {
        size_t end = source_.find('<', pos_);
        if (end == std::string::npos) {
            Token t{TokenType::Character, source_.substr(pos_), {}};
            pos_ = source_.size();
            return t;
        }
        Token t{TokenType::Character, source_.substr(pos_, end - pos_), {}};
        pos_ = end;
        return t;
    }
};
// DOMNodePool is now defined in include/html/DOMNode.h and implemented in src/html/DOMNode.cpp

HTMLParser::HTMLParser() : nodePool_(new DOMNodePool()) {}

HTMLParser::~HTMLParser() {
    delete nodePool_;
}

DOMNode* HTMLParser::parse(const std::string& html) {
    nodePool_->reset();

    auto tokens = Tokenizer(html).tokenize();

    Document* doc = nodePool_->createDocument();
    DOMNode* stack_[256];
    int stackTop = 0;
    stack_[stackTop++] = doc;

    for (const Token& token : tokens) {
        if (token.type == TokenType::EOF_TOKEN) {
            break;
        }

        if (token.type == TokenType::DOCTYPE) {
            continue;
        }

        if (token.type == TokenType::Comment) {
            DOMNode* comment = nodePool_->createNode(NodeType::Comment);
            comment->textContent = token.data;
            stack_[stackTop - 1]->appendChild(comment);
            continue;
        }

        if (token.type == TokenType::Character) {
            if (!token.data.empty()) {
                DOMNode* text = nodePool_->createNode(NodeType::Text);
                text->textContent = token.data;
                stack_[stackTop - 1]->appendChild(text);
            }
            continue;
        }

        if (token.type == TokenType::StartTag) {
            DOMNode* element = nodePool_->createNode(NodeType::Element);
            element->tagName = token.data;
            for (const auto& attr : token.attributes) {
                element->attributes[attr.first] = attr.second;
            }

            stack_[stackTop - 1]->appendChild(element);

            // Handle void elements - they have no children and don't stay on stack
            if (!isVoidElement(token.data)) {
                stack_[stackTop++] = element;
            }
            continue;
        }

        if (token.type == TokenType::EndTag) {
            if (stackTop > 1) {
                // Pop current element, move to parent
                stackTop--;
            }
            continue;
        }
    }

    return doc;
}

} // namespace html