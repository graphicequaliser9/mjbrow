/**
 * @file Selectors.h
 * @brief CSS selector scaffolding.
 * @details Holds the grammar-level types the cascade engine uses to match
 *          elements against rule selector lists.  Full specificity rules
 *          and index-compatible matching are implemented in bead 4.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_SELECTORS_H
#define CSS_SELECTORS_H

#include <string>
#include <vector>

namespace css {

/**
 * @struct SimpleSelector
 * @brief One leaf selector inside a compound selector chain.
 *         e.g. "div.foo#bar[href]" yields three SimpleSelector records.
 */
struct SimpleSelector {
    enum Combinator { Descendant, Child, AdajacentSibling, GeneralSibling };
    Combinator combinator{Descendant};

    std::string tagName;           ///< "*" (universal), "div", etc.
    std::string id;                ///< #id selector; empty if absent.
    std::vector<std::string> classes; ///< .class tokens.

    /// @brief Attribute tests: [attr=value], [attr~=value], etc.
    struct AttrTest {
        std::string name;
        std::string op;   ///< "=", "~=", "|=", "^=", "$=", "*=", or "" for existence.
        std::string value;
        bool operator==(const AttrTest& other) const {
            return name == other.name && op == other.op && value == other.value;
        }
    };
    std::vector<AttrTest> attrs;

    bool operator==(const SimpleSelector& other) const {
        return combinator == other.combinator
            && tagName    == other.tagName
            && id         == other.id
            && classes    == other.classes
            && attrs      == other.attrs;
    }
};

} // namespace css

#endif // CSS_SELECTORS_H