/**
 * @file CSSParser.cpp
 * @brief CSS parser implementation.
 * @details Tokenizes CSS text and parses rule sets (selectors + declarations)
 *          and at-rules (@media, @font-face, ...).  Also exposes an inline-style
 *          parser for HTML style="" attributes.  Cascade / inheritance is out of
 *          scope here (see Bead B).
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/CSSParser.h"
#include "css/Selectors.h"
#include "css/ComputedStyle.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace css {

namespace {

bool isIdentStart(int c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
           c > 127;  // allow non-ASCII (UTF-8) in identifiers
}

bool isIdentChar(int c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
           c > 127;
}

bool isHexDigit(int c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

bool isWhitespace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool isDigit(int c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

// Tokenizer state: a cursor over the input with an index + length.
struct Lexer {
    const std::string& src;
    size_t i = 0;

    explicit Lexer(const std::string& s) : src(s) {}

    int peek(size_t off = 0) const {
        return (i + off < src.size()) ? static_cast<unsigned char>(src[i + off]) : -1;
    }

    int get() { return (i < src.size()) ? static_cast<unsigned char>(src[i++]) : -1; }

    void skipWhitespace() {
        while (isWhitespace(peek())) ++i;
    }

    bool skipWhitespaceAndComments() {
        bool any = false;
        for (;;) {
            size_t before = i;
            skipWhitespace();
            if (i != before) any = true;
            if (peek() == '/' && peek(1) == '*') {
                i += 2;
                any = true;
                while (i < src.size() && !(src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/'))
                    ++i;
                if (i < src.size()) i += 2;  // consume */
            } else {
                break;
            }
        }
        return any;
    }
};

CSSToken makeToken(CSSTokenType t, const std::string& text = "",
                   double number = 0.0, const std::string& unit = "") {
    CSSToken tok;
    tok.type = t;
    tok.text = text;
    tok.number = number;
    tok.unit = unit;
    return tok;
}

// Serialise a token back to its source text.  Used to reconstruct selector
// strings and declaration values from the token stream.
std::string tokenToText(const CSSToken& t) {
    switch (t.type) {
        case CSSTokenType::Ident:      return t.text;
        case CSSTokenType::AtKeyword:  return "@" + t.text;
        case CSSTokenType::String:     return "\"" + t.text + "\"";
        case CSSTokenType::Hash:       return "#" + t.text;
        case CSSTokenType::Function:   return t.text + "(" + t.unit + ")";
        case CSSTokenType::Url:        return "url(" + t.text + ")";
        case CSSTokenType::Number: {
            std::ostringstream os;
            os << t.number;
            return os.str();
        }
        case CSSTokenType::Dimension: {
            std::ostringstream os;
            os << t.number << t.unit;
            return os.str();
        }
        case CSSTokenType::LParen:     return "(";
        case CSSTokenType::RParen:     return ")";
        case CSSTokenType::LBrace:     return "{";
        case CSSTokenType::RBrace:     return "}";
        case CSSTokenType::LBracket:   return "[";
        case CSSTokenType::RBracket:   return "]";
        case CSSTokenType::Colon:      return ":";
        case CSSTokenType::Semicolon:  return ";";
        case CSSTokenType::Comma:      return ",";
        case CSSTokenType::Whitespace: return " ";
        case CSSTokenType::Delim:      return t.text;
        default:                       return "";
    }
}

// Join a run of tokens into a single source string, inserting separators only
// where CSS grammar requires them.  This keeps type/class/id markers attached
// (div.box#id), preserves the descendant combinator (whitespace), and avoids
// spurious spaces around grouping delimiters.
std::string joinTokens(const std::vector<CSSToken>& toks) {
    std::string out;
    CSSTokenType prev = CSSTokenType::EOF_TOKEN;
    for (const auto& t : toks) {
        if (t.type == CSSTokenType::Whitespace) {
            // Collapse multiple spaces; never lead/trail with one for our callers.
            if (!out.empty() && out.back() != ' ') out.push_back(' ');
            prev = CSSTokenType::Whitespace;
            continue;
        }
        bool needSpace = false;
        if (prev != CSSTokenType::EOF_TOKEN) {
            bool prevAttach = (prev == CSSTokenType::LParen ||
                               prev == CSSTokenType::Comma ||
                               prev == CSSTokenType::Colon ||
                               prev == CSSTokenType::Delim);
            bool curAttach = (t.type == CSSTokenType::RParen ||
                              t.type == CSSTokenType::RBrace ||
                              t.type == CSSTokenType::RBracket ||
                              t.type == CSSTokenType::Comma ||
                              t.type == CSSTokenType::Delim ||
                              t.type == CSSTokenType::Colon ||
                              t.type == CSSTokenType::Hash);
            bool prevWhitespace = (prev == CSSTokenType::Whitespace);
            needSpace = !prevAttach && !curAttach && !prevWhitespace;
        }
        if (needSpace) out.push_back(' ');
        out += tokenToText(t);
        prev = t.type;
    }
    // Trim leading/trailing whitespace.
    size_t a = out.find_first_not_of(" \t\r\n\f");
    size_t z = out.find_last_not_of(" \t\r\n\f");
    if (a == std::string::npos) return "";
    return out.substr(a, z - a + 1);
}

// Read a CSS string literal (single or double quoted), handling escapes.
std::string readString(Lexer& L, int quote) {
    std::string out;
    L.get();  // consume opening quote
    for (;;) {
        int c = L.peek();
        if (c == -1 || c == quote) {
            if (c == quote) L.get();
            break;
        }
        if (c == '\\') {
            L.get();
            int nxt = L.peek();
            if (nxt == '\n' || nxt == '\r') {
                L.get();  // line continuation
                if (nxt == '\r' && L.peek() == '\n') L.get();
            } else if (nxt != -1) {
                out.push_back(static_cast<char>(L.get()));
            }
        } else {
            out.push_back(static_cast<char>(L.get()));
        }
    }
    return out;
}

// Consume a run of identifier characters (and CSS escapes) starting at i.
std::string readIdent(Lexer& L) {
    std::string out;
    for (;;) {
        int c = L.peek();
        if (c == '\\') {
            L.get();  // consume backslash
            int nxt = L.peek();
            if (nxt != -1) out.push_back(static_cast<char>(L.get()));
            continue;
        }
        if (isIdentChar(c)) {
            out.push_back(static_cast<char>(L.get()));
        } else {
            break;
        }
    }
    return out;
}

// Parse a number (optionally signed, with decimal point / exponent).
// Returns false if no number starts here.
bool readNumber(Lexer& L, double& value, std::string& unit) {
    size_t start = L.i;
    if (L.peek() == '+' || L.peek() == '-') L.get();
    bool sawDigit = false;
    while (isDigit(L.peek())) { L.get(); sawDigit = true; }
    if (L.peek() == '.') {
        L.get();
        while (isDigit(L.peek())) { L.get(); sawDigit = true; }
    }
    if (!sawDigit) { L.i = start; return false; }
    // exponent
    if (L.peek() == 'e' || L.peek() == 'E') {
        size_t save = L.i; L.get();
        if (L.peek() == '+' || L.peek() == '-') L.get();
        if (isDigit(L.peek())) {
            while (isDigit(L.peek())) L.get();
        } else {
            L.i = save;  // not an exponent
        }
    }
    std::string numStr = L.src.substr(start, L.i - start);
    value = std::strtod(numStr.c_str(), nullptr);
    // unit: identifier immediately following with no separator
    unit.clear();
    if (isIdentStart(L.peek())) {
        unit = readIdent(L);
    }
    return true;
}

std::vector<CSSToken> doTokenize(const std::string& css) {
    Lexer L(css);
    std::vector<CSSToken> out;

    for (;;) {
        bool skippedWS = L.skipWhitespaceAndComments();
        if (skippedWS && !out.empty()) out.push_back(makeToken(CSSTokenType::Whitespace));
        int c = L.peek();
        if (c == -1) break;

        switch (c) {
            case '{': L.get(); out.push_back(makeToken(CSSTokenType::LBrace)); break;
            case '}': L.get(); out.push_back(makeToken(CSSTokenType::RBrace)); break;
            case '(': L.get(); out.push_back(makeToken(CSSTokenType::LParen)); break;
            case ')': L.get(); out.push_back(makeToken(CSSTokenType::RParen)); break;
            case '[': L.get(); out.push_back(makeToken(CSSTokenType::LBracket)); break;
            case ']': L.get(); out.push_back(makeToken(CSSTokenType::RBracket)); break;
            case ':': L.get(); out.push_back(makeToken(CSSTokenType::Colon)); break;
            case ';': L.get(); out.push_back(makeToken(CSSTokenType::Semicolon)); break;
            case ',': L.get(); out.push_back(makeToken(CSSTokenType::Comma)); break;

            case '#': {
                L.get();
                // Capture a #id selector name OR a #hex colour.  Both are a run
                // of ident/hex characters; we keep them verbatim.
                std::string name;
                while (isHexDigit(L.peek()) || isIdentChar(L.peek())) {
                    name.push_back(static_cast<char>(L.get()));
                }
                out.push_back(makeToken(CSSTokenType::Hash, name));
                break;
            }

            case '@': {
                L.get();  // consume '@'
                std::string name = readIdent(L);
                out.push_back(makeToken(CSSTokenType::AtKeyword, name));
                break;
            }

            case '"':
            case '\'': {
                out.push_back(makeToken(CSSTokenType::String, readString(L, c)));
                break;
            }

            default: {
                if (c == '.' || c == '+' || c == '-' ||
                    (isDigit(c) && (c != '0' || !isIdentStart(L.peek(1))))) {
                    // Try a number / dimension first.
                    double value = 0;
                    std::string unit;
                    if (readNumber(L, value, unit)) {
                        if (unit.empty())
                            out.push_back(makeToken(CSSTokenType::Number, "", value));
                        else
                            out.push_back(makeToken(CSSTokenType::Dimension, "", value, unit));
                        break;
                    }
                }
                if (isIdentStart(c)) {
                    std::string ident = readIdent(L);
                    if (L.peek() == '(') {
                        // function token: rgb( / url( / rgba( / hsl( ...
                        L.get();  // consume '('
                        // Parse the argument list until matching ')'.
                        std::string args;
                        int depth = 1;
                        while (L.peek() != -1 && depth > 0) {
                            int ch = L.peek();
                            if (ch == '(') ++depth;
                            else if (ch == ')') { --depth; if (depth == 0) { L.get(); break; } }
                            if (ch == '"' || ch == '\'') {
                                args.push_back(static_cast<char>(ch));
                                args += readString(L, ch);
                                args.push_back(static_cast<char>(ch));
                                continue;
                            }
                            args.push_back(static_cast<char>(L.get()));
                        }
                        // Trim surrounding whitespace of args.
                        size_t a0 = args.find_first_not_of(" \t\r\n\f");
                        size_t a1 = args.find_last_not_of(" \t\r\n\f");
                        if (a0 == std::string::npos) args.clear();
                        else args = args.substr(a0, a1 - a0 + 1);

                        // url() is special: treat as a Url token with the inner text.
                        if (ident == "url") {
                            // Strip optional quotes.
                            std::string uri = args;
                            if (uri.size() >= 2 &&
                                ((uri.front() == '"' && uri.back() == '"') ||
                                 (uri.front() == '\'' && uri.back() == '\''))) {
                                uri = uri.substr(1, uri.size() - 2);
                            }
                            out.push_back(makeToken(CSSTokenType::Url, uri));
                        } else {
                            CSSToken tok = makeToken(CSSTokenType::Function, ident);
                            tok.unit = args;  // stash args in `unit` field
                            out.push_back(tok);
                        }
                        break;
                    }
                    out.push_back(makeToken(CSSTokenType::Ident, ident));
                    break;
                }
                // Any other character is a delimiter.
                L.get();
                out.push_back(makeToken(CSSTokenType::Delim, std::string(1, static_cast<char>(c))));
                break;
            }
        }
    }

    out.push_back(makeToken(CSSTokenType::EOF_TOKEN));
    return out;
}

} // namespace

// ── CSSParser ──────────────────────────────────────────────────────────────────

CSSParser::CSSParser() = default;
CSSParser::~CSSParser() = default;

std::vector<CSSToken> CSSParser::tokenize(const std::string& css) {
    return doTokenize(css);
}

// Collect declaration pairs ("property: value;") from a token run.  Used for
// both the body of a style rule and for inline style="" attributes.
std::map<std::string, std::string> collectDeclarations(const std::vector<CSSToken>& toks) {
    std::map<std::string, std::string> decls;
    std::vector<CSSToken> valueToks;
    std::string property;
    bool pending = false;

    for (const auto& t : toks) {
        if (t.type == CSSTokenType::Ident && !pending) {
            property = t.text;
            pending = true;
        } else if (t.type == CSSTokenType::Colon && pending) {
            // start accumulating the value
            valueToks.clear();
        } else if (t.type == CSSTokenType::Semicolon ||
                   t.type == CSSTokenType::RBrace ||
                   t.type == CSSTokenType::EOF_TOKEN) {
            if (pending && !valueToks.empty()) {
                decls[property] = joinTokens(valueToks);
            }
            valueToks.clear();
            property.clear();
            pending = false;
        } else if (pending) {
            // value token (skip the colon that started the value)
            valueToks.push_back(t);
        }
    }
    return decls;
}

std::map<std::string, std::string> CSSParser::parseInlineStyle(const std::string& styleAttr) {
    std::vector<CSSToken> toks = doTokenize(styleAttr);
    return collectDeclarations(toks);
}

std::vector<CSSRule> CSSParser::parse(const std::string& css) {
    std::vector<CSSRule> rules;
    std::vector<CSSToken> toks = doTokenize(css);

    size_t idx = 0;
    const size_t n = toks.size();

    auto isSelectorToken = [&](CSSTokenType ty) {
        return ty == CSSTokenType::Ident || ty == CSSTokenType::Hash ||
               ty == CSSTokenType::Delim ||
               ty == CSSTokenType::Number || ty == CSSTokenType::Dimension ||
               ty == CSSTokenType::String || ty == CSSTokenType::LParen ||
               ty == CSSTokenType::Function || ty == CSSTokenType::Colon ||
               ty == CSSTokenType::LBracket || ty == CSSTokenType::Comma ||
               ty == CSSTokenType::Url;
    };

    while (idx < n) {
        const CSSToken& t = toks[idx];

        // Skip stray semicolons / braces that may appear between rules.
        if (t.type == CSSTokenType::Semicolon || t.type == CSSTokenType::RBrace) {
            ++idx;
            continue;
        }

        if (t.type == CSSTokenType::AtKeyword) {
            // At-rule: @name prelude { body }  (or @import "x";)
            CSSRule rule;
            rule.type = (t.text == "import") ? RuleType::Import : RuleType::At;

            ++idx;  // move past @keyword
            // Collect prelude tokens until '{' (block) or ';' (statement).
            std::vector<CSSToken> preludeToks;
            bool blockBody = false;
            while (idx < n) {
                const CSSToken& p = toks[idx];
                if (p.type == CSSTokenType::LBrace) { blockBody = true; ++idx; break; }
                if (p.type == CSSTokenType::Semicolon) { ++idx; break; }
                if (p.type == CSSTokenType::EOF_TOKEN) break;
                preludeToks.push_back(p);
                ++idx;
            }
            rule.selector = joinTokens(preludeToks);
            // Prefix the selector with '@' + keyword for clarity.
            rule.selector = "@" + t.text + (rule.selector.empty() ? "" : " " + rule.selector);

            if (blockBody) {
                // Gather the nested block tokens until the matching '}'.
                int depth = 1;
                std::vector<CSSToken> bodyToks;
                while (idx < n && depth > 0) {
                    const CSSToken& b = toks[idx];
                    if (b.type == CSSTokenType::LBrace) ++depth;
                    else if (b.type == CSSTokenType::RBrace) {
                        --depth;
                        if (depth == 0) { ++idx; break; }
                    }
                    bodyToks.push_back(b);
                    ++idx;
                }

                // Some at-rules (e.g. @font-face, @page) hold plain declarations;
                // others (e.g. @media, @supports, @keyframes) hold a nested rule
                // set.  Decide based on the keyword.
                static const char* kDeclarationBodies[] = {
                    "font-face", "page", "viewport", "counter-style",
                    "font-feature-values", nullptr};
                bool declBody = false;
                for (const char** kw = kDeclarationBodies; *kw; ++kw) {
                    if (t.text == *kw) { declBody = true; break; }
                }

                if (declBody) {
                    rule.declarations = collectDeclarations(bodyToks);
                } else {
                    // Recursively parse the nested block (a stylesheet of its own).
                    std::string bodySrc;
                    for (const auto& bt : bodyToks) {
                        if (bt.type == CSSTokenType::EOF_TOKEN) break;
                        bodySrc += tokenToText(bt);
                        bodySrc.push_back(' ');
                    }
                    rule.nested = parse(bodySrc);
                }
            }
            rules.push_back(std::move(rule));
            continue;
        }

        // Style rule: collect the selector list until '{', then declarations.
        if (isSelectorToken(t.type)) {
            std::vector<CSSToken> selectorToks;
            bool sawLBrace = false;
            while (idx < n) {
                const CSSToken& s = toks[idx];
                if (s.type == CSSTokenType::LBrace) { sawLBrace = true; ++idx; break; }
                if (s.type == CSSTokenType::EOF_TOKEN) break;
                selectorToks.push_back(s);
                ++idx;
            }
            if (!sawLBrace) {
                // No body (malformed trailing selector); stop.
                break;
            }
            std::string selector = joinTokens(selectorToks);

            // Collect declaration tokens until the matching '}'.
            std::vector<CSSToken> declToks;
            while (idx < n) {
                const CSSToken& d = toks[idx];
                if (d.type == CSSTokenType::RBrace) { ++idx; break; }
                if (d.type == CSSTokenType::EOF_TOKEN) break;
                declToks.push_back(d);
                ++idx;
            }

            CSSRule rule;
            rule.type = RuleType::Style;
            rule.selector = selector;
            rule.declarations = collectDeclarations(declToks);
            rules.push_back(std::move(rule));
            continue;
        }

        // Anything else: advance to avoid infinite loop.
        ++idx;
    }

    return rules;
}

} // namespace css
