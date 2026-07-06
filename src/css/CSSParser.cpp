/**
 * @file CSSParser.cpp
 * @brief Full CSS3 tokenizer and stylesheet parser.
 * @details Implements tokenizer, selector parser, shorthand property expansion,
 *          and StyleSheet / Rule / KeyFrame structure construction.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/CSSParser.h"
#include "css/Selectors.h"
#include "css/ComputedStyle.h"
#include "util/Logging.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace css {

// ── StringPool ──────────────────────────────────────────────────────────────

const char* StringPool::intern(const std::string& str) {
    auto it = index_.find(str);
    if (it != index_.end()) return strings_[it->second].c_str();
    size_t idx = strings_.size();
    strings_.push_back(str);
    index_[strings_.back()] = idx;
    return strings_.back().c_str();
}

const char* StringPool::intern(const char* data, size_t len) {
    std::string tmp(data, len);
    return intern(tmp);
}

size_t StringPool::bytesUsed() const {
    size_t total = 0;
    for (const auto& s : strings_) total += s.size() + sizeof(std::string);
    return total;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < s.size()) {
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        if (start >= s.size()) break;
        size_t end = start;
        while (end < s.size() && !std::isspace(static_cast<unsigned char>(s[end]))) end++;
        parts.emplace_back(s.substr(start, end - start));
        start = end;
    }
    return parts;
}

static const std::unordered_set<std::string> kBorderStyles = {
    "none", "hidden", "dotted", "dashed", "solid", "double",
    "groove", "ridge", "inset", "outset"
};

static bool isBorderStyle(const std::string& token) {
    return kBorderStyles.count(token) > 0;
}

static bool isBorderWidth(const std::string& token) {
    if (token == "thin" || token == "medium" || token == "thick") return true;
    if (token == "0") return true;
    char* end = nullptr;
    std::strtof(token.c_str(), &end);
    return end && *end == '\0';
}

static bool isLengthOrAuto(const std::string& token) {
    if (token == "auto") return true;
    if (token == "0") return true;
    if (token.size() > 0 && token.back() == '%') return true;
    if (token.size() > 1) {
        char c = token.back();
        if (c == 'x' || c == 'y' || c == 'z' || c == 'p' || c == 'e' || c == 'm' || c == 'x' || token == "px" || token == "em" || token == "rem" || token == "vh" || token == "vw" || token == "vmin" || token == "vmax" || token == "pt" || token == "cm" || token == "mm" || token == "in") return true;
    }
    char* end = nullptr;
    std::strtof(token.c_str(), &end);
    return end != nullptr && *end == '\0';
}

// ── CSSParser ctor / dtor ───────────────────────────────────────────────────

CSSParser::CSSParser() = default;
CSSParser::~CSSParser() = default;

void CSSParser::reset() {
    css_.clear();
    pos_ = 0;
    line_ = 1;
    col_ = 1;
    tokens_.clear();
    tokenPos_ = 0;
    pushback_.clear();
    pool_.clear();
    errorMsg_.clear();
}

// ── Tokenizer state machine ──────────────────────────────────────────────────

void CSSParser::tokenize(const std::string& css) {
    css_ = css;
    pos_ = 0;
    line_ = 1;
    col_ = 1;
    tokens_.clear();
    tokenPos_ = 0;

    while (pos_ < css_.size()) {
        skipWhitespace();
        skipComment();
        skipWhitespace();

        if (pos_ >= css_.size()) break;

        size_t startLine = line_;
        size_t startCol = col_;
        char c = css_[pos_];

        if (c == '\0') {
            tokens_.push_back(Token{TokenType::EOF_, "", startLine, startCol});
            break;
        }

        // String tokens (quoted)
        if (c == '"' || c == '\'') {
            char quote = c;
            pos_++; col_++;
            size_t start = pos_;
            while (pos_ < css_.size() && css_[pos_] != quote) {
                if (css_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
                pos_++;
                if (css_[pos_] == '\\' && pos_ + 1 < css_.size()) { pos_++; col_++; }
            }
            tokens_.push_back(Token{TokenType::String,
                                   pool_.intern(css_.c_str() + start, pos_ - start),
                                   startLine, startCol});
            if (pos_ < css_.size()) { pos_++; col_++; }
            continue;
        }

        // Numbers, percentages, dimensions
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && pos_ + 1 < css_.size() && std::isdigit(static_cast<unsigned char>(css_[pos_ + 1])))) {
            size_t start = pos_;
            while (pos_ < css_.size() && std::isdigit(static_cast<unsigned char>(css_[pos_]))) {
                col_++; pos_++;
            }
            if (pos_ < css_.size() && css_[pos_] == '.') {
                pos_++; col_++;
                while (pos_ < css_.size() && std::isdigit(static_cast<unsigned char>(css_[pos_]))) {
                    col_++; pos_++;
                }
            }
            // scientific notation
            if (pos_ + 1 < css_.size() && (css_[pos_] == 'e' || css_[pos_] == 'E')) {
                size_t ePos = pos_;
                pos_++; col_++;
                if (pos_ < css_.size() && (css_[pos_] == '+' || css_[pos_] == '-')) { pos_++; col_++; }
                while (pos_ < css_.size() && std::isdigit(static_cast<unsigned char>(css_[pos_]))) {
                    col_++; pos_++;
                }
                if (pos_ == ePos + 1 || (pos_ == ePos + 2 && (css_[ePos+1] == '+' || css_[ePos+1] == '-'))) {
                    pos_ = ePos; col_ -= (pos_ - ePos);
                }
            }
            // percentage
            if (pos_ < css_.size() && css_[pos_] == '%') {
                tokens_.push_back(Token{TokenType::Percentage,
                                       pool_.intern(css_.c_str() + start, pos_ - start + 1),
                                       startLine, startCol});
                pos_++; col_++;
                continue;
            }
            // dimension (e.g., 10px)
            if (pos_ < css_.size() && std::isalpha(static_cast<unsigned char>(css_[pos_]))) {
                size_t dimStart = start;
                while (pos_ < css_.size() && (std::isalnum(static_cast<unsigned char>(css_[pos_])) || css_[pos_] == '_' || css_[pos_] == '-')) {
                    col_++; pos_++;
                }
                tokens_.push_back(Token{TokenType::Dimension,
                                       pool_.intern(css_.c_str() + dimStart, pos_ - dimStart),
                                       startLine, startCol});
                continue;
            }
            tokens_.push_back(Token{TokenType::Number,
                                   pool_.intern(css_.c_str() + start, pos_ - start),
                                   startLine, startCol});
            continue;
        }

        // Two-character tokens
        if (pos_ + 1 < css_.size()) {
            char two[3] = {css_[pos_], css_[pos_ + 1], '\0'};
            if (strcmp(two, "/*") == 0) {
                pos_ += 2; col_ += 2;
                while (pos_ + 1 < css_.size() && !(css_[pos_] == '*' && css_[pos_ + 1] == '/')) {
                    if (css_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
                    pos_++;
                }
                if (pos_ + 1 < css_.size()) { pos_ += 2; col_ += 2; }
                continue;
            }
            if (strcmp(two, "~=") == 0) {
                tokens_.push_back(Token{TokenType::Includes, "~=", startLine, startCol});
                pos_ += 2; col_ += 2; continue;
            }
            if (strcmp(two, "|=") == 0) {
                tokens_.push_back(Token{TokenType::DashMatch, "|=", startLine, startCol});
                pos_ += 2; col_ += 2; continue;
            }
            if (strcmp(two, "^=") == 0) {
                tokens_.push_back(Token{TokenType::PrefixMatch, "^=", startLine, startCol});
                pos_ += 2; col_ += 2; continue;
            }
            if (strcmp(two, "$=") == 0) {
                tokens_.push_back(Token{TokenType::SuffixMatch, "$=", startLine, startCol});
                pos_ += 2; col_ += 2; continue;
            }
            if (strcmp(two, "*=") == 0) {
                tokens_.push_back(Token{TokenType::SubstringMatch, "*=", startLine, startCol});
                pos_ += 2; col_ += 2; continue;
            }
            if (strcmp(two, "//") == 0) {
                while (pos_ < css_.size() && css_[pos_] != '\n') { col_++; pos_++; }
                continue;
            }
        }

        // Structural delimiters
        switch (c) {
            case '{': tokens_.push_back(Token{TokenType::LBrace, "{", startLine, startCol}); pos_++; col_++; continue;
            case '}': tokens_.push_back(Token{TokenType::RBrace, "}", startLine, startCol}); pos_++; col_++; continue;
            case '(': tokens_.push_back(Token{TokenType::LParen, "(", startLine, startCol}); pos_++; col_++; continue;
            case ')': tokens_.push_back(Token{TokenType::RParen, ")", startLine, startCol}); pos_++; col_++; continue;
            case '[': tokens_.push_back(Token{TokenType::LBracket, "[", startLine, startCol}); pos_++; col_++; continue;
            case ']': tokens_.push_back(Token{TokenType::RBracket, "]", startLine, startCol}); pos_++; col_++; continue;
            case ':': tokens_.push_back(Token{TokenType::Colon, ":", startLine, startCol}); pos_++; col_++; continue;
            case ';': tokens_.push_back(Token{TokenType::Semicolon, ";", startLine, startCol}); pos_++; col_++; continue;
            case ',': tokens_.push_back(Token{TokenType::Comma, ",", startLine, startCol}); pos_++; col_++; continue;
            case '.': tokens_.push_back(Token{TokenType::Dot, ".", startLine, startCol}); pos_++; col_++; continue;
            case '#': tokens_.push_back(Token{TokenType::Hash, "#", startLine, startCol}); pos_++; col_++; continue;
            case '@': tokens_.push_back(Token{TokenType::Hash, "@", startLine, startCol}); pos_++; col_++; continue;
            case '=': tokens_.push_back(Token{TokenType::Equals, "=", startLine, startCol}); pos_++; col_++; continue;
            case '*': tokens_.push_back(Token{TokenType::Star, "*", startLine, startCol}); pos_++; col_++; continue;
            default: break;
        }

        // Identifiers
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            size_t start = pos_;
            while (pos_ < css_.size() && (std::isalnum(static_cast<unsigned char>(css_[pos_])) || css_[pos_] == '_' || css_[pos_] == '-')) {
                col_++; pos_++;
            }
            std::string ident(css_, start, pos_ - start);
            tokens_.push_back(Token{TokenType::Ident, pool_.intern(ident), startLine, startCol});
            continue;
        }

        // Anything else is a delimiter
        tokens_.push_back(Token{TokenType::Delim, pool_.intern(std::string(1, c)), startLine, startCol});
        pos_++; col_++;
    }

    if (!tokens_.empty() && tokens_.back().type != TokenType::EOF_) {
        tokens_.push_back(Token{TokenType::EOF_, "", line_, col_});
    } else if (tokens_.empty()) {
        tokens_.push_back(Token{TokenType::EOF_, "", line_, col_});
    }
}

// ── Token stream wrappers ────────────────────────────────────────────────────

Token CSSParser::nextToken() {
    if (!pushback_.empty()) {
        Token t = pushback_.back();
        pushback_.pop_back();
        return t;
    }
    if (tokenPos_ >= tokens_.size()) return Token{TokenType::EOF_, "", line_, col_};
    return tokens_[tokenPos_++];
}

Token CSSParser::consumeToken() {
    return nextToken();
}

void CSSParser::pushBack(const Token& tok) {
    pushback_.push_back(tok);
}

bool CSSParser::match(TokenType type) {
    Token t = nextToken();
    if (t.type == type) return true;
    pushBack(t);
    return false;
}

bool CSSParser::match(const char* text) {
    Token t = nextToken();
    if (t.text == text) return true;
    pushBack(t);
    return false;
}

bool CSSParser::isEOF() const {
    if (!pushback_.empty()) return false;
    return tokenPos_ >= tokens_.size() ||
           (tokenPos_ < tokens_.size() && tokens_[tokenPos_].type == TokenType::EOF_);
}

char CSSParser::current() const {
    if (pos_ < css_.size()) return css_[pos_];
    return '\0';
}

void CSSParser::advance() {
    if (pos_ < css_.size()) {
        if (css_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
        pos_++;
    }
}

void CSSParser::skipWhitespace() {
    while (pos_ < css_.size() && std::isspace(static_cast<unsigned char>(css_[pos_]))) {
        if (css_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
        pos_++;
    }
}

bool CSSParser::skipComment() {
    if (pos_ + 1 >= css_.size()) return false;
    if (css_[pos_] != '/' || css_[pos_ + 1] != '*') return false;
    pos_ += 2;
    col_ += 2;
    while (pos_ + 1 < css_.size()) {
        if (css_[pos_] == '*' && css_[pos_ + 1] == '/') {
            pos_ += 2;
            col_ += 2;
            return true;
        }
        if (css_[pos_] == '\n') { line_++; col_ = 1; } else { col_++; }
        pos_++;
    }
    return true;
}

// ── StyleSheet parsing ───────────────────────────────────────────────────────

StyleSheet CSSParser::parse(const std::string& css) {
    reset();
    tokenize(css);
    StyleSheet sheet;
    parseStyleSheet();
    sheet.rules = parseRuleList(sheet);
    if (!errorMsg_.empty()) sheet.error = errorMsg_;
    sheet.valid = !sheet.rules.empty() || !sheet.keyframes.empty();
    return sheet;
}

StyleSheet CSSParser::parseStyleSheet() {
    return StyleSheet{};
}

std::vector<CSSRule> CSSParser::parseRuleList(StyleSheet& sheet) {
    std::vector<CSSRule> rules;
    KeyFrameRule keyframe;

    while (!isEOF()) {
        skipWhitespace();
        skipComment();
        if (isEOF()) break;

        Token t = consumeToken();
        if (t.type == TokenType::Hash && t.text == "@") {
            pushBack(t);
            if (parseAtRule(keyframe)) {
                if (keyframe.valid) sheet.keyframes.push_back(keyframe);
                keyframe = KeyFrameRule{};
            }
            continue;
        }

        if (t.type == TokenType::EOF_) break;

        // Re-push token to try as a selector or at-rule
        pushBack(t);

        if (t.type == TokenType::Ident && t.text[0] == '@') {
            if (parseAtRule(keyframe)) {
                if (keyframe.valid) sheet.keyframes.push_back(keyframe);
                keyframe = KeyFrameRule{};
            }
            continue;
        }

        CSSRule rule;
        if (parseRule(rule)) {
            rules.push_back(rule);
        }
    }

    return rules;
}

bool CSSParser::parseRule(CSSRule& rule) {
    std::vector<SimpleSelector> selectors;
    if (!parseSelectorList(selectors)) return false;

    if (!match(TokenType::LBrace)) return false;

    std::vector<std::pair<const char*, const char*>> decls;
    if (!parseDeclarations(decls)) {
        if (!match(TokenType::RBrace)) return false;
    }

    if (!match(TokenType::RBrace)) {
        return false;
    }

    for (auto& sel : selectors) {
        rule.selectors.push_back(sel);
    }
    for (auto& decl : decls) {
        rule.declarations.emplace_back(decl.first, decl.second);
    }
    rule.type = RuleType::Style;
    return true;
}

bool CSSParser::parseAtRule(KeyFrameRule& keyframe) {
    Token at = consumeToken();
    if (at.type != TokenType::Hash || at.text != "@") return false;

    Token nameTok = consumeToken();
    if (nameTok.type != TokenType::Ident) {
        pushBack(nameTok);
        return false;
    }

    std::string name = nameTok.text;

    if (name == "keyframes" || name == "-webkit-keyframes" || name == "-moz-keyframes") {
        Token frameName = consumeToken();
        std::string kfName;
        if (frameName.type == TokenType::Ident) kfName = frameName.text;
        else if (frameName.type == TokenType::String) kfName = frameName.text;
        else { pushBack(frameName); return false; }

        if (!match(TokenType::LBrace)) return false;

        keyframe.name = pool_.intern(kfName);
        bool firstBlock = true;

        while (!isEOF() && !match(TokenType::RBrace)) {
            if (!firstBlock) break;
            firstBlock = false;

            float percent = 0.0f;
            Token selTok = consumeToken();

            std::string selText = selTok.text;
            if (selText == "from") { percent = 0.0f; }
            else if (selText == "to") { percent = 100.0f; }
            else if (selTok.type == TokenType::Percentage) {
                percent = std::stof(selTok.text);
            }
            else if (selTok.type == TokenType::Number) {
                percent = std::stof(selTok.text);
            }
            else if (selTok.type == TokenType::Ident && selText.find('%') != std::string::npos) {
                percent = std::stof(selText);
            }
            else {
                pushBack(selTok);
                break;
            }

            if (!match(TokenType::LBrace)) break;

            std::vector<std::pair<const char*, const char*>> keyDecls;
            if (!parseDeclarations(keyDecls)) break;

            if (!match(TokenType::RBrace)) break;

            keyframe.keyframes.emplace_back(percent, std::move(keyDecls));
        }
        keyframe.valid = !keyframe.keyframes.empty();
        return keyframe.valid;
    }

    // Other at-rules: consume block if present
    if (match(TokenType::LBrace)) {
        int depth = 1;
        while (!isEOF() && depth > 0) {
            Token t = consumeToken();
            if (t.type == TokenType::LBrace) depth++;
            else if (t.type == TokenType::RBrace) depth--;
        }
    }

    return true;
}

// ── Selector parsing ─────────────────────────────────────────────────────────

bool CSSParser::parseSelectorList(std::vector<SimpleSelector>& out) {
    out.clear();
    SimpleSelector sel;
    if (!parseSimpleSelector(sel)) return false;
    out.push_back(sel);

    while (match(TokenType::Comma)) {
        SimpleSelector next;
        if (parseSimpleSelector(next)) {
            out.push_back(next);
        } else {
            break;
        }
    }
    return true;
}

bool CSSParser::parseSimpleSelector(SimpleSelector& out) {
    out = SimpleSelector{};
    out.tagName = "";
    out.id = "";
    out.classes.clear();
    out.attrs.clear();

    Token t = consumeToken();
    if (t.type == TokenType::Ident) {
        out.tagName = pool_.intern(t.text);
    } else if (t.type == TokenType::Star) {
        out.tagName = pool_.intern("*");
    } else if (t.type == TokenType::Hash) {
        out.id = pool_.intern(t.text);
    } else if (t.type == TokenType::Dot) {
        out.tagName = pool_.intern("*");
        pushBack(t);
    } else if (t.type == TokenType::LBracket) {
        out.tagName = pool_.intern("*");
        pushBack(t);
    } else {
        pushBack(t);
        out.tagName = pool_.intern("*");
        return true;
    }

    while (!isEOF()) {
        skipWhitespace();
        Token m = consumeToken();

        if (m.type == TokenType::Dot) {
            Token classTok = consumeToken();
            if (classTok.type == TokenType::Ident) {
                out.classes.push_back(pool_.intern(classTok.text));
            } else {
                pushBack(classTok);
                break;
            }
        } else if (m.type == TokenType::Hash) {
            Token idTok = consumeToken();
            if (idTok.type == TokenType::HashIdent) {
                out.id = pool_.intern(idTok.text);
            } else if (idTok.type == TokenType::Ident) {
                out.id = pool_.intern(idTok.text);
            } else {
                pushBack(idTok);
                break;
            }
        } else if (m.type == TokenType::LBracket) {
            SimpleSelector::AttrTest attr;
            Token attrName = consumeToken();
            if (attrName.type != TokenType::Ident) {
                pushBack(attrName);
                break;
            }
            attr.name = pool_.intern(attrName.text);

            Token opTok = consumeToken();
            if (opTok.type == TokenType::RBracket) {
                attr.op = "";
                attr.value = "";
                out.attrs.push_back(attr);
                continue;
            }

            if (opTok.type == TokenType::Equals) {
                attr.op = "=";
            } else if (opTok.type == TokenType::Includes) {
                attr.op = "~=";
            } else if (opTok.type == TokenType::DashMatch) {
                attr.op = "|=";
            } else if (opTok.type == TokenType::PrefixMatch) {
                attr.op = "^=";
            } else if (opTok.type == TokenType::SuffixMatch) {
                attr.op = "$=";
            } else if (opTok.type == TokenType::SubstringMatch) {
                attr.op = "*=";
            } else {
                attr.op = "";
                attr.value = "";
                out.attrs.push_back(attr);
                pushBack(opTok);
                continue;
            }

            Token valTok = consumeToken();
            if (valTok.type == TokenType::Ident || valTok.type == TokenType::String || valTok.type == TokenType::Number) {
                attr.value = pool_.intern(valTok.text);
            } else {
                attr.value = pool_.intern(valTok.text);
            }

            if (!match(TokenType::RBracket)) {
                pushBack(valTok);
                break;
            }
            out.attrs.push_back(attr);
        } else if (m.type == TokenType::Colon) {
            pushBack(m);
            break;
        } else if (m.type == TokenType::Comma || m.type == TokenType::LBrace || m.type == TokenType::EOF_) {
            pushBack(m);
            break;
        } else if (m.type == TokenType::Ident || m.type == TokenType::Star) {
            pushBack(m);
            break;
        } else {
            pushBack(m);
            break;
        }
    }

    return true;
}

// ── Declaration parsing ──────────────────────────────────────────────────────

bool CSSParser::parseDeclarations(std::vector<std::pair<const char*, const char*>>& out) {
    while (!isEOF()) {
        skipWhitespace();
        skipComment();
        if (match(TokenType::RBrace)) { pushBack(Token{TokenType::RBrace, "}", line_, col_}); break; }
        if (match(TokenType::EOF_)) break;

        std::pair<const char*, const char*> decl;
        if (!parseDeclaration(decl)) break;
        out.emplace_back(decl.first, decl.second);

        skipWhitespace();
        if (!match(TokenType::Semicolon)) {
            if (match(TokenType::RBrace)) {
                pushBack(Token{TokenType::RBrace, "}", line_, col_});
                break;
            }
        }
    }
    return true;
}

bool CSSParser::parseDeclaration(std::pair<const char*, const char*>& out) {
    Token propTok = consumeToken();
    if (propTok.type != TokenType::Ident) {
        pushBack(propTok);
        return false;
    }

    const char* property = pool_.intern(propTok.text);

    // Consume optional colon
    if (!match(TokenType::Colon)) return false;

    // Consume value tokens until we hit a terminator
    std::string value;
    bool first = true;
    while (!isEOF()) {
        skipWhitespace();
        if (match(TokenType::Semicolon)) {
            pushBack(Token{TokenType::Semicolon, ";", line_, col_});
            break;
        }
        if (match(TokenType::RBrace)) {
            pushBack(Token{TokenType::RBrace, "}", line_, col_});
            break;
        }
        if (match(TokenType::EOF_)) break;

        Token v = consumeToken();
        if (v.type == TokenType::EOF_) break;

        if (!first) value.push_back(' ');
        value += v.text;
        first = false;
    }

    // Expand shorthands
    std::vector<ShorthandExpansion> expansions;
    if (expandShorthand(property, value.c_str(), expansions)) {
        for (const auto& exp : expansions) {
            out.first = exp.property;
            out.second = exp.value;
        }
    } else {
        out.first = property;
        out.second = pool_.intern(value);
    }

    return true;
}

// ── Value and function parsing ───────────────────────────────────────────────

bool CSSParser::parseValue(std::string& out) {
    out.clear();
    if (isEOF()) return false;

    Token t = consumeToken();
    if (t.type == TokenType::String || t.type == TokenType::Ident ||
        t.type == TokenType::Number || t.type == TokenType::Percentage ||
        t.type == TokenType::Dimension) {
        out = t.text;
        return true;
    }
    if (t.type == TokenType::LParen) {
        return parseFunction(out);
    }
    pushBack(t);
    return false;
}

bool CSSParser::parseFunction(std::string& out) {
    Token name = consumeToken();
    if (name.type != TokenType::Ident) {
        pushBack(name);
        return false;
    }
    out = name.text;
    out.push_back('(');

    while (!isEOF()) {
        skipWhitespace();
        if (match(TokenType::RParen)) { out.push_back(')'); return true; }
        if (match(TokenType::Comma))   { out.push_back(','); continue; }

        Token arg = consumeToken();
        if (arg.type == TokenType::String || arg.type == TokenType::Ident ||
            arg.type == TokenType::Number || arg.type == TokenType::Percentage ||
            arg.type == TokenType::Dimension) {
            if (!out.empty() && out.back() != '(' && out.back() != ',') out.push_back(' ');
            out += arg.text;
        } else if (arg.type == TokenType::LParen) {
            out.push_back('(');
        } else if (arg.type == TokenType::RParen) {
            out.push_back(')');
        } else {
            pushBack(arg);
            break;
        }
    }
    if (out.back() != ')') out.push_back(')');
    return true;
}

void CSSParser::parseUntilSemicolon(std::string& out) {
    out.clear();
    bool first = true;
    while (!isEOF()) {
        skipWhitespace();
        if (match(TokenType::Semicolon)) break;
        if (match(TokenType::RBrace)) { pushBack(Token{TokenType::RBrace, "}", line_, col_}); break; }
        if (match(TokenType::EOF_)) break;

        Token t = consumeToken();
        if (t.type == TokenType::EOF_) break;
        if (!first) out.push_back(' ');
        out += t.text;
        first = false;
    }
}

// ── Shorthand expansion ──────────────────────────────────────────────────────

bool CSSParser::expandShorthand(const char* property, const char* value,
                                std::vector<ShorthandExpansion>& out) {
    std::string prop = property;
    std::string val = value ? value : "";

    if (prop == "margin")  return expandMargin(val, out);
    if (prop == "padding") return expandPadding(val, out);
    if (prop == "border" || prop == "border-top" || prop == "border-right" ||
        prop == "border-bottom" || prop == "border-left" || prop == "outline")
        return expandBorder(val, out);
    if (prop == "background") return expandBackground(val, out);
    if (prop == "border-radius") return expandBorderRadius(val, out);
    return false;
}

bool CSSParser::expandMargin(const std::string& value, std::vector<ShorthandExpansion>& out) {
    auto parts = split(value);
    const char* base = pool_.intern("margin");
    if (parts.empty()) return false;

    std::string v0 = pool_.intern(parts[0]);
    std::string v1 = parts.size() > 1 ? pool_.intern(parts[1]) : v0;
    std::string v2 = parts.size() > 2 ? pool_.intern(parts[2]) : v0;
    std::string v3 = parts.size() > 3 ? pool_.intern(parts[3]) : v1;

    out.push_back({pool_.intern(std::string(base) + "-top"), v0.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-right"), v1.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-bottom"), v2.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-left"), v3.c_str()});
    return true;
}

bool CSSParser::expandPadding(const std::string& value, std::vector<ShorthandExpansion>& out) {
    auto parts = split(value);
    const char* base = pool_.intern("padding");
    if (parts.empty()) return false;

    std::string v0 = pool_.intern(parts[0]);
    std::string v1 = parts.size() > 1 ? pool_.intern(parts[1]) : v0;
    std::string v2 = parts.size() > 2 ? pool_.intern(parts[2]) : v0;
    std::string v3 = parts.size() > 3 ? pool_.intern(parts[3]) : v1;

    out.push_back({pool_.intern(std::string(base) + "-top"), v0.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-right"), v1.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-bottom"), v2.c_str()});
    out.push_back({pool_.intern(std::string(base) + "-left"), v3.c_str()});
    return true;
}

bool CSSParser::expandBorder(const std::string& value, std::vector<ShorthandExpansion>& out) {
    auto parts = split(value);
    std::string width, style, color;
    for (const auto& p : parts) {
        if (isBorderStyle(p) && style.empty()) {
            style = p;
        } else if (isBorderWidth(p) && width.empty()) {
            width = p;
        } else if (color.empty() && !p.empty() && (p[0] == '#' || p.find("rgb") == 0)) {
            color = p;
        } else {
            if (width.empty() && isLengthOrAuto(p)) width = p;
            else if (style.empty() && isBorderStyle(p)) style = p;
            else if (color.empty()) color = p;
        }
    }
    if (!width.empty()) { ShorthandExpansion e{"border-width", pool_.intern(width)}; out.push_back(e); }
    if (!style.empty()) { ShorthandExpansion e{"border-style", pool_.intern(style)}; out.push_back(e); }
    if (!color.empty()) { ShorthandExpansion e{"border-color", pool_.intern(color)}; out.push_back(e); }
    return true;
}

bool CSSParser::expandBackground(const std::string& value, std::vector<ShorthandExpansion>& out) {
    ShorthandExpansion e{"background", pool_.intern(value)};
    out.push_back(e);
    return true;
}

bool CSSParser::expandBorderRadius(const std::string& value, std::vector<ShorthandExpansion>& out) {
    auto parts = split(value);
    if (parts.empty()) return false;

    std::string v0 = pool_.intern(parts[0]);
    std::string v1 = parts.size() > 1 ? pool_.intern(parts[1]) : v0;
    std::string v2 = parts.size() > 2 ? pool_.intern(parts[2]) : v0;
    std::string v3 = parts.size() > 3 ? pool_.intern(parts[3]) : v1;

    out.push_back({"border-top-left-radius",     v0.c_str()});
    out.push_back({"border-top-right-radius",    v1.c_str()});
    out.push_back({"border-bottom-right-radius", v2.c_str()});
    out.push_back({"border-bottom-left-radius",  v3.c_str()});
    return true;
}

// ── Hashing ──────────────────────────────────────────────────────────────────

size_t CSSParser::hashSelector(const std::vector<SimpleSelector>& selectors) const {
    size_t h = 0x9e3779b9;
    for (const auto& sel : selectors) {
        h ^= std::hash<std::string>{}(sel.tagName) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(sel.id) + 0x9e3779b9 + (h << 6) + (h >> 2);
        for (const auto& cls : sel.classes) {
            h ^= std::hash<std::string>{}(cls) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        for (const auto& attr : sel.attrs) {
            h ^= std::hash<std::string>{}(attr.name) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>{}(attr.op) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>{}(attr.value) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
    }
    return h;
}

} // namespace css
