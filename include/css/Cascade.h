#ifndef CSS_CASCADE_H
#define CSS_CASCADE_H

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "css/CSSParser.h"
#include "css/Selectors.h"
#include "css/ComputedStyle.h"

namespace html { class DOMNode; }

namespace css {

struct MediaCondition {
    int minWidth{-1};
    int maxWidth{INT_MAX};

    bool matches(int viewportWidth) const {
        if (minWidth >= 0 && viewportWidth < minWidth) return false;
        return viewportWidth <= maxWidth;
    }
};

class RuleMatcher {
public:
    explicit RuleMatcher(const StyleSheet& sheet);
    ~RuleMatcher();

    void buildIndex();

    bool matchesSelector(const SimpleSelector& sel, const html::DOMNode* element) const;
    bool matchesAnySelector(const CSSRule& rule, const html::DOMNode* element) const;

    std::vector<size_t> getCandidates(const html::DOMNode* element) const;

    size_t computeSpecificity(const SimpleSelector& sel) const;
    size_t computeRuleSpecificity(const CSSRule& rule) const;

private:
    const StyleSheet* sheet_;

    std::unordered_multimap<std::string, size_t> tagIndex_;
    std::unordered_multimap<std::string, size_t> idIndex_;
    std::unordered_multimap<std::string, size_t> classIndex_;

    std::string getClassesAttribute(const html::DOMNode* element) const;
    bool matchAttribute(const SimpleSelector::AttrTest& test, const std::string& value) const;
    bool tagMatches(const std::string& selectorTag, const html::DOMNode* element) const;
    bool classMatches(const std::vector<std::string>& selectorClasses, const html::DOMNode* element) const;
    bool idMatches(const std::string& selectorId, const html::DOMNode* element) const;
    bool attrMatches(const std::vector<SimpleSelector::AttrTest>& selectorAttrs, const html::DOMNode* element) const;
};

class Cascade {
public:
    struct ResolvedMediaSheet {
        MediaCondition condition;
        StyleSheet sheet;
    };

    explicit Cascade(std::vector<ResolvedMediaSheet> mediaSheets, int viewportWidth = 1024);
    ~Cascade();

    void resolve(html::DOMNode* document);

private:
    std::vector<ResolvedMediaSheet> mediaSheets_;
    int viewportWidth_;
    std::vector<ComputedStyle*> allocatedStyles_;

    void applyCascadeToElement(html::DOMNode* element);
    void applyInlineStyles(html::DOMNode* element);
    void applyDeclarations(html::DOMNode* element,
                           const std::vector<std::pair<const char*, const char*>>& declarations);
    void parseProperty(html::DOMNode* element, const char* prop, const char* val);

    static float parseLength(const std::string& val, float base = 0.0f);
    static uint32_t parseColor(const std::string& val);
    static ComputedStyle::Display parseDisplay(const std::string& val);
    static ComputedStyle::Position parsePosition(const std::string& val);
    static ComputedStyle::Float parseFloat(const std::string& val);
    static ComputedStyle::BoxSizing parseBoxSizing(const std::string& val);
    static ComputedStyle::Overflow parseOverflow(const std::string& val);
    static std::vector<std::string> splitClasses(const std::string& classAttr);
};

class MediaParser {
public:
    static std::vector<Cascade::ResolvedMediaSheet> expandMediaRules(const std::string& css);
};

} // namespace css

#endif // CSS_CASCADE_H
