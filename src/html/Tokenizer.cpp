/**
 * @file html/Tokenizer.cpp
 * @brief HTML5 tokeniser implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/Tokenizer.h"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace html {

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (!atEnd()) {
        if (input_[pos_] == '<') {
            ++pos_;
            if (atEnd()) break;

            if (input_[pos_] == '/') {
                // End tag
                ++pos_;
                skipWhitespace();
                std::string tag = readTagName();
                tokens.push_back({TokenType::EndTag, tag, {}});
                while (!atEnd() && input_[pos_] != '>') ++pos_;
                if (!atEnd()) ++pos_;
            } else if (input_[pos_] == '!') {
                ++pos_;
                if (!atEnd() && input_[pos_] == '-' &&
                    pos_ + 1 < input_.size() && input_[pos_ + 1] == '-') {
                    // Comment
                    pos_ += 2;
                    std::string comment = readCommentText();
                    tokens.push_back({TokenType::Comment, comment, {}});
                } else {
                    // DOCTYPE or bogus comment
                    std::string decl = readBogusComment();
                    size_t i = 0;
                    auto skipWs = [&](size_t& p) {
                        while (p < decl.size() && std::isspace(static_cast<unsigned char>(decl[p]))) ++p;
                    };
                    auto readWord = [&](size_t& p) -> std::string {
                        size_t s = p;
                        while (p < decl.size() && !std::isspace(static_cast<unsigned char>(decl[p]))) ++p;
                        return decl.substr(s, p - s);
                    };
                    skipWs(i);
                    std::string first = toLower(readWord(i));
                    std::string name;
                    if (first == "doctype") { skipWs(i); name = readWord(i); }
                    tokens.push_back({TokenType::DOCTYPE, name, {}});
                }
            } else {
                // Start tag
                std::string tag = readTagName();
                Token tok{TokenType::StartTag, tag, {}};
                readAttributes(tok);

                if (!atEnd() && input_[pos_] == '/') {
                    tok.selfClosing = true;
                    ++pos_;
                }
                while (!atEnd() && input_[pos_] != '>') ++pos_;
                if (!atEnd()) ++pos_;

                tokens.push_back(tok);

                // Raw-text elements: consume content until matching end tag.
                if (!tok.selfClosing && isRawTextElement(tag)) {
                    std::string endTagLower = toLower(tag);
                    std::string content = readRawTextContent(endTagLower);
                    tokens.push_back({TokenType::Character, content, {}});
                }
            }
        } else {
            // Character data
            size_t start = pos_;
            while (!atEnd() && input_[pos_] != '<') ++pos_;
            if (pos_ > start) {
                tokens.push_back({TokenType::Character, input_.substr(start, pos_ - start), {}});
            }
        }
    }

    tokens.push_back({TokenType::EOF_TOKEN, "", {}});
    return tokens;
}

std::string Tokenizer::readTagName() {
    size_t start = pos_;
    while (!atEnd() &&
           !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
           input_[pos_] != '>' && input_[pos_] != '/') {
        ++pos_;
    }
    return toLower(input_.substr(start, pos_ - start));
}

void Tokenizer::readAttributes(Token& tok) {
    while (!atEnd()) {
        skipWhitespace();
        if (atEnd() || input_[pos_] == '>' || input_[pos_] == '/') break;

        size_t nameStart = pos_;
        while (!atEnd() &&
               !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
               input_[pos_] != '=' && input_[pos_] != '>' && input_[pos_] != '/') {
            ++pos_;
        }
        std::string name = toLower(input_.substr(nameStart, pos_ - nameStart));

        skipWhitespace();
        std::string value;
        if (!atEnd() && input_[pos_] == '=') {
            ++pos_;
            skipWhitespace();
            if (!atEnd() && (input_[pos_] == '"' || input_[pos_] == '\'')) {
                char quote = input_[pos_++];
                size_t valStart = pos_;
                while (!atEnd() && input_[pos_] != quote) ++pos_;
                value = input_.substr(valStart, pos_ - valStart);
                if (!atEnd()) ++pos_;
            } else {
                size_t valStart = pos_;
                while (!atEnd() &&
                       !std::isspace(static_cast<unsigned char>(input_[pos_])) &&
                       input_[pos_] != '>' && input_[pos_] != '/') {
                    ++pos_;
                }
                value = input_.substr(valStart, pos_ - valStart);
            }
        }
        tok.attributes.emplace_back(name, value);
    }
}

std::string Tokenizer::readCommentText() {
    size_t start = pos_;
    while (pos_ + 2 < input_.size() &&
           !(input_[pos_] == '-' && input_[pos_ + 1] == '-' && input_[pos_ + 2] == '>')) {
        ++pos_;
    }
    std::string comment = input_.substr(start, pos_ - start);
    if (pos_ + 2 < input_.size()) pos_ += 3;
    return comment;
}

std::string Tokenizer::readBogusComment() {
    size_t start = pos_;
    while (!atEnd() && input_[pos_] != '>') ++pos_;
    std::string s = input_.substr(start, pos_ - start);
    if (!atEnd()) ++pos_;
    return s;
}

std::string Tokenizer::readRawTextContent(const std::string& endTagLower) {
    size_t start = pos_;
    while (!atEnd()) {
        if (input_[pos_] == '<' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '/') {
            size_t checkPos = pos_ + 2;
            size_t tagLen = 0;
            while (checkPos < input_.size() &&
                   !std::isspace(static_cast<unsigned char>(input_[checkPos])) &&
                   input_[checkPos] != '>') {
                if (tagLen < endTagLower.size() &&
                    static_cast<char>(std::tolower(static_cast<unsigned char>(input_[checkPos]))) == endTagLower[tagLen]) {
                    ++tagLen;
                    ++checkPos;
                } else {
                    break;
                }
            }
            if (tagLen == endTagLower.size() &&
                (checkPos >= input_.size() ||
                 std::isspace(static_cast<unsigned char>(input_[checkPos])) ||
                 input_[checkPos] == '>')) {
                break;
            }
        }
        ++pos_;
    }
    std::string content = input_.substr(start, pos_ - start);
    return content;
}

} // namespace html
