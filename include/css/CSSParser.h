/**
 * @file CSSParser.h
 * @brief CSS3 parser scaffolding.
 * @details Tokeniser for @media, @keyframes, @supports, calc(), var(),
 *          hsl/hsla/rgba, 4/8-digit hex, quoted urls.  Produces a flat list of
 *          CSSRules using the selector types declared in include/css/Selectors.h.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_CSSPARSER_H
#define CSS_CSSPARSER_H

#include <string>
#include <vector>

#include "css/Selectors.h"

namespace css {

enum class RuleType {
    Style,
    At,
    Import,
    Keyframe,
};

/**
 * @struct CSSRule
 * @brief One parsed CSS rule (style or at-rule).
 */
struct CSSRule {
    RuleType type{RuleType::Style};
    std::vector<SimpleSelector> selectors;
    std::vector<std::pair<std::string, std::string>> declarations; ///< property / value
};

class CSSParser {
public:
    CSSParser();
    ~CSSParser();

    /// @brief Parses CSS input and returns the list of rules.
    /// @param css The CSS string to parse.
    /// @return Parsed CSS rules.
    std::vector<CSSRule> parse(const std::string& css);

private:
    // Placeholder for internal state
};

} // namespace css

#endif // CSS_CSSPARSER_H