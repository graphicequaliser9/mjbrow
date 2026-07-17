/**
 * @file css/Cascade.h
 * @brief CSS cascade engine (Bead B).
 * @details Implements selector matching and the cascade for one DOM element.
 *          Given a set of CSS rules (produced by the parser in Bead A) and a
 *          target DOMNode, it computes the final map<string,string> of
 *          declaration values by:
 *            - matching the rule selector against the element (and its
 *              ancestors for combinator selectors),
 *            - ordering matched rules by specificity then source order,
 *            - merging declarations so the highest-priority rule wins.
 *          Inline styles (style="") are treated as the highest specificity
 *          after author rules.  :visited / :link / :hover pseudo-classes are
 *          accepted syntactically but treated as no-ops (always matched).
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_CASCADE_H
#define CSS_CASCADE_H

#include <map>
#include <string>
#include <vector>

#include "css/CSSParser.h"
#include "html/DOMNode.h"

namespace css {

/**
 * @brief Specificity of a matching selector.
 * @details Encoded as (inline, id, class, type) where each field counts the
 *          number of components of that kind.  Comparison is lexicographic:
 *          inline > id > class > type > source order.  Pseudo-classes count as
 *          a class component for now.
 */
struct Specificity {
    int inlineCount{0};  ///< 1 if the declarations came from an inline style.
    int idCount{0};      ///< number of #id components.
    int classCount{0};   ///< number of .class and :pseudo-class components.
    int typeCount{0};    ///< number of type (tag) and * components.
    int sourceOrder{0};  ///< index of the rule in the stylesheet (tie-breaker).

    bool operator<(const Specificity& o) const {
        if (inlineCount != o.inlineCount) return inlineCount < o.inlineCount;
        if (idCount    != o.idCount)    return idCount    < o.idCount;
        if (classCount != o.classCount) return classCount < o.classCount;
        if (typeCount  != o.typeCount)  return typeCount  < o.typeCount;
        return sourceOrder < o.sourceOrder;
    }
    bool operator==(const Specificity& o) const {
        return inlineCount == o.inlineCount && idCount == o.idCount &&
               classCount == o.classCount && typeCount == o.typeCount &&
               sourceOrder == o.sourceOrder;
    }
};

/**
 * @struct MatchedRule
 * @brief A rule that matched a given element, annotated with its specificity
 *        and source order so the cascade can order it correctly.
 */
struct MatchedRule {
    const CSSRule* rule{nullptr};  ///< originating rule (nullptr for inline).
    std::map<std::string, std::string> declarations;  ///< property -> value.
    Specificity specificity;        ///< ordering key.

    bool operator<(const MatchedRule& o) const {
        return specificity < o.specificity;
    }
};

class Cascade {
public:
    /**
     * @brief Computes the final resolved declaration map for @p element.
     * @param element The DOM element to compute styles for.
     * @param rules   Author style rules (already parse()'d), in source order.
     * @return A map of property name -> resolved value.  Later / more specific
     *         rules override earlier ones.  Inline style wins over author rules.
     */
    static std::map<std::string, std::string>
    computeStyle(const html::DOMNode* element, const std::vector<CSSRule>& rules);

    /**
     * @brief Computes specificity for a single selector string (no combinator
     *        expansion / no element).  Useful for testing and for callers that
     *        need to rank selectors directly.
     * @param selector A single (complex) selector, e.g. "div.box#main > p".
     */
    static Specificity specificityOf(const std::string& selector);

    /**
     * @brief Returns true if @p selector matches @p element.
     * @details Supports type, .class, #id, universal (*), descendant and child
     *          combinators, and the :visited / :link / :hover pseudo-classes
     *          (treated as no-ops for now).
     */
    static bool matches(const html::DOMNode* element, const std::string& selector);

private:
    // Helpers live in the .cpp.
};

} // namespace css

#endif // CSS_CASCADE_H
