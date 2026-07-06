/**
 * @file CSSParser.h
 * @brief Full CSS3 tokenizer and stylesheet parser.
 * @details Parses rules, selectors (element, .class, #id, [attr]), shorthand
 *          properties (margin, padding, border, background), and @keyframes.
 *          Produces a StyleSheet containing CSSRules and KeyFrameRules.
 *          Strings are interned via StringPool to keep memory usage low.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_CSSPARSER_H
#define CSS_CSSPARSER_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "css/StringPool.h"
#include "css/Selectors.h"

namespace css {

// ── Rule types ───────────────────────────────────────────────────────────────

enum class RuleType {
    Style,
    At,
    Import,
    Keyframe,
};

// ── CSSRule (defined before StyleSheet since StyleSheet references it) ────────

/**
 * @struct CSSRule
 * @brief One parsed CSS rule (style or at-rule).
 */
struct CSSRule {
    RuleType type{RuleType::Style};
    std::vector<SimpleSelector> selectors;
    std::vector<std::pair<const char*, const char*>> declarations;
};

// ── Token types ─────────────────────────────────────────────────────────────

enum class TokenType {
    Ident,
    Hash,
    HashIdent,
    Dot,
    Colon,
    Comma,
    Semicolon,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    String,
    Number,
    Percentage,
    Dimension,
    Delim,
    EOF_,
    CDO,
    CDC,
    Includes,
    DashMatch,
    PrefixMatch,
    SuffixMatch,
    SubstringMatch,
    Star,
    Equals,
};

struct Token {
    TokenType type{TokenType::EOF_};
    std::string text;
    size_t line{0};
    size_t column{0};
};

// ── Shorthand expansion ─────────────────────────────────────────────────────

struct ShorthandExpansion {
    const char* property;
    const char* value;
};

// ── KeyFrame ────────────────────────────────────────────────────────────────

struct KeyFrameRule {
    std::string name;
    std::vector<std::pair<float, std::vector<std::pair<const char*, const char*>>>> keyframes;
    bool valid{false};
};

// ── StyleSheet ──────────────────────────────────────────────────────────────

struct StyleSheet {
    std::vector<CSSRule> rules;
    std::vector<KeyFrameRule> keyframes;
    bool valid{false};
    std::string error;
};

// ── Parser ──────────────────────────────────────────────────────────────────

class CSSParser {
public:
    CSSParser();
    ~CSSParser();

    CSSParser(const CSSParser&) = delete;
    CSSParser& operator=(const CSSParser&) = delete;

    StyleSheet parse(const std::string& css);
    void reset();

    const char* lastError() const { return errorMsg_.empty() ? nullptr : errorMsg_.c_str(); }

private:
    // ── Tokenizer ──────────────────────────────────────────────────────────
    void tokenize(const std::string& css);
    Token nextToken();
    Token consumeToken();
    void pushBack(const Token& tok);
    bool match(TokenType type);
    bool match(const char* text);
    bool isEOF() const;
    char current() const;
    void advance();
    void skipWhitespace();
    bool skipComment();

    // ── Stylesheet parsing ─────────────────────────────────────────────────
    StyleSheet parseStyleSheet();
    bool parseAtRule(KeyFrameRule& keyframe);
    bool parseRule(CSSRule& rule);
    std::vector<CSSRule> parseRuleList(StyleSheet& sheet);

    // ── Selector parsing ───────────────────────────────────────────────────
    bool parseSelectorList(std::vector<SimpleSelector>& out);
    bool parseSimpleSelector(SimpleSelector& out);
    bool parsePseudoClassSelector(SimpleSelector& out);

    // ── Declaration parsing ────────────────────────────────────────────────
    bool parseDeclarations(std::vector<std::pair<const char*, const char*>>& out);
    bool parseDeclaration(std::pair<const char*, const char*>& out);
    bool parseValue(std::string& out);
    bool parseFunction(std::string& out);
    void parseUntilSemicolon(std::string& out);

    // ── Shorthand expansion ────────────────────────────────────────────────
    bool expandShorthand(const char* property, const char* value,
                         std::vector<ShorthandExpansion>& out);
    bool expandMargin(const std::string& value, std::vector<ShorthandExpansion>& out);
    bool expandPadding(const std::string& value, std::vector<ShorthandExpansion>& out);
    bool expandBorder(const std::string& value, std::vector<ShorthandExpansion>& out);
    bool expandBackground(const std::string& value, std::vector<ShorthandExpansion>& out);
    bool expandBorderRadius(const std::string& value, std::vector<ShorthandExpansion>& out);

    // ── Hashing ────────────────────────────────────────────────────────────
    size_t hashSelector(const std::vector<SimpleSelector>& selectors) const;

    // ── State ──────────────────────────────────────────────────────────────
    std::string css_;
    size_t pos_{0};
    size_t line_{1};
    size_t col_{1};
    std::vector<Token> tokens_;
    size_t tokenPos_{0};
    std::vector<Token> pushback_;
    StringPool pool_;
    std::string errorMsg_;
};

} // namespace css

#endif // CSS_CSSPARSER_H
