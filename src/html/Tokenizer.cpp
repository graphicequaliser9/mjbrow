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

static std::string decodeHtmlEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&' && i + 1 < s.size()) {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos) {
                std::string ent = s.substr(i + 1, semi - i - 1);
                if (ent == "nbsp") { out += '\xa0'; i = semi; continue; }
                if (ent == "amp")  { out += '&';  i = semi; continue; }
                if (ent == "lt")   { out += '<';  i = semi; continue; }
                if (ent == "gt")   { out += '>';  i = semi; continue; }
                if (ent == "quot") { out += '"';  i = semi; continue; }
                if (ent == "apos") { out += '\''; i = semi; continue; }
                if (ent == "copy") { out += '\xa9'; i = semi; continue; }
                if (ent == "reg")  { out += '\xae'; i = semi; continue; }
                if (ent == "trade"){ out += '\x99'; i = semi; continue; }
                if (ent == "mdash"){ out += '\x97'; i = semi; continue; }
                if (ent == "ndash"){ out += '\x96'; i = semi; continue; }
                if (ent == "hellip"){ out += '\x85'; i = semi; continue; }
                if (ent == "lsquo"){ out += '\x91'; i = semi; continue; }
                if (ent == "rsquo"){ out += '\x92'; i = semi; continue; }
                if (ent == "ldquo"){ out += '\x93'; i = semi; continue; }
                if (ent == "rdquo"){ out += '\x94'; i = semi; continue; }
                if (ent == "bull") { out += '\x95'; i = semi; continue; }
                if (ent == "OElig"){ out += '\x8c'; i = semi; continue; }
                if (ent == "oelig"){ out += '\x9c'; i = semi; continue; }
                if (ent == "Scaron"){ out += '\x8a'; i = semi; continue; }
                if (ent == "scaron"){ out += '\x9a'; i = semi; continue; }
                if (ent == "Yuml") { out += '\x9f'; i = semi; continue; }
                if (ent == "fnof") { out += '\x83'; i = semi; continue; }
                if (ent == "circ") { out += '\x88'; i = semi; continue; }
                if (ent == "tilde"){ out += '\x98'; i = semi; continue; }
                if (ent == "ensp") { out += '\x89'; i = semi; continue; }
                if (ent == "emsp") { out += '\x93'; i = semi; continue; }
                if (ent == "thinsp"){ out += '\x89'; i = semi; continue; }
                if (ent == "zwnj") { out += '\x8c'; i = semi; continue; }
                if (ent == "zwj")  { out += '\x9d'; i = semi; continue; }
                if (ent == "lrm")  { out += '\x8d'; i = semi; continue; }
                if (ent == "rlm")  { out += '\x8e'; i = semi; continue; }
                if (ent == "sbquo"){ out += '\x9a'; i = semi; continue; }
                if (ent == "bdquo"){ out += '\x9f'; i = semi; continue; }
                if (ent == "dagger"){ out += '\x86'; i = semi; continue; }
                if (ent == "Dagger"){ out += '\x87'; i = semi; continue; }
                if (ent == "permil"){ out += '\x89'; i = semi; continue; }
                if (ent == "lsaquo"){ out += '\xb9'; i = semi; continue; }
                if (ent == "rsaquo"){ out += '\xbb'; i = semi; continue; }
                if (ent == "euro")  { out += '\x80'; i = semi; continue; }
                if (ent.size() > 1 && ent[0] == '#') {
                    int val = 0;
                    if (ent[1] == 'x' || ent[1] == 'X') {
                        val = std::stoi(ent.substr(2), nullptr, 16);
                    } else {
                        val = std::stoi(ent.substr(1), nullptr, 10);
                    }
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    out += static_cast<char>(val);
                    i = semi; continue;
                }
            }
        }
        out += s[i];
    }
    return out;
}

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
                    tokens.push_back({TokenType::Character, decodeHtmlEntities(content), {}});
                }
            }
        } else {
            // Character data
            size_t start = pos_;
            while (!atEnd() && input_[pos_] != '<') ++pos_;
            if (pos_ > start) {
                tokens.push_back({TokenType::Character, decodeHtmlEntities(input_.substr(start, pos_ - start)), {}});
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
        if (!value.empty()) value = decodeHtmlEntities(value);
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
