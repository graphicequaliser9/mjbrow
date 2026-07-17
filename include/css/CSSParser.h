/**
 * @file CSSParser.h
 * @brief CSS parser: tokenizer + rule-set / inline-style parser.
 * @details Implements a basic CSS parser producing a flat list of CSSRules.
 *          Each rule carries its raw selector string and a map of property ->
 *          value declarations.  The tokenizer recognises identifiers, strings,
 *          numbers, dimensions, hash colours, url() and functional values such
 *          as rgb()/rgba().  At-rules (@media, @font-face, ...) are captured as
 *          their own rules.  Inline styles (style="...") are parsed through the
 *          same declaration machinery.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_CSSPARSER_H
#define CSS_CSSPARSER_H

#include <map>
#include <string>
#include <vector>

#include "css/Selectors.h"

namespace css {

// ── Tokenizer ─────────────────────────────────────────────────────────────────

enum class CSSTokenType {
    Ident,        ///< identifier (property, value keyword, function name)
    String,       ///< "double" or 'single' quoted string
    Number,       ///< 12, 0.5, -3
    Dimension,    ///< number + unit suffix, e.g. 24px, 50% (unit in `unit`)
    Hash,         ///< #fff or #ffffff colour
    Function,     ///< rgb(  / url(  (name in `text`; args follow until ')')
    Url,          ///< url(...) literal (resolved url in `text`)
    LParen,       ///< (
    RParen,       ///< )
    LBrace,       ///< {
    RBrace,       ///< }
    LBracket,     ///< [
    RBracket,     ///< ]
    Colon,        ///< :
    Semicolon,    ///< ;
    Comma,        ///< ,
    Delim,        ///< misc single char (>, +, ~, *)
    Whitespace,   ///< run of spaces between tokens (descendant combinator / list sep)
    AtKeyword,    ///< @media / @font-face (name without '@' in `text`)
    EOF_TOKEN,
};

struct CSSToken {
    CSSTokenType type{CSSTokenType::EOF_TOKEN};
    std::string text;       ///< raw text for ident/string/function/hash/at-keyword
    double number{0.0};     ///< numeric value for Number / Dimension
    std::string unit;       ///< unit suffix for Dimension (e.g. "px", "%")
};

// ── Rule types ───────────────────────────────────────────────────────────────

enum class RuleType {
    Style,   ///< selector { declarations }
    At,      ///< @media / @font-face / @keyframes etc.
    Import,  ///< @import "...";
};

/**
 * @struct CSSRule
 * @brief One parsed CSS rule.
 * @details `selector` holds the raw selector text (e.g. "div.foo > p"). For
 *          at-rules the selector field holds the at-keyword name plus any
 *          prelude (e.g. "media (min-width: 600px)"). `declarations` maps each
 *          property name to its value string.
 */
struct CSSRule {
    RuleType type{RuleType::Style};

    /// @brief Raw selector text (style rules) or at-rule prelude (at-rules).
    std::string selector;

    /// @brief property -> value map for the rule's declarations.
    std::map<std::string, std::string> declarations;

    /// @brief Nested rules for at-rules such as @media (empty for style rules).
    std::vector<CSSRule> nested;
};

class CSSParser {
public:
    CSSParser();
    ~CSSParser();

    /// @brief Parses a full CSS stylesheet and returns the list of rules.
    /// @param css The CSS source string.
    /// @return Parsed CSS rules (style rules and at-rules).
    std::vector<CSSRule> parse(const std::string& css);

    /// @brief Parses the contents of an HTML style="" attribute.
    /// @param styleAttr The raw attribute value, e.g. "color: red; font-size: 24px".
    /// @return property -> value map of inline declarations.
    static std::map<std::string, std::string> parseInlineStyle(const std::string& styleAttr);

    /// @brief Tokenizes CSS text (exposed for testing / reuse).
    static std::vector<CSSToken> tokenize(const std::string& css);

private:
    // Placeholder for internal state
};

} // namespace css

#endif // CSS_CSSPARSER_H
