#include "css/Cascade.h"
#include "html/DOMNode.h"
#include "css/Selectors.h"
#include "css/CSSParser.h"
#include "util/Logging.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace css {

using ResolvedSheet = Cascade::ResolvedMediaSheet;

// ── Helpers ─────────────────────────────────────────────────────────────────────

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

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ── RuleMatcher ────────────────────────────────────────────────────────────────

RuleMatcher::RuleMatcher(const StyleSheet& sheet) : sheet_(&sheet) {}

RuleMatcher::~RuleMatcher() = default;

void RuleMatcher::buildIndex() {
    tagIndex_.clear();
    idIndex_.clear();
    classIndex_.clear();

    for (size_t i = 0; i < sheet_->rules.size(); ++i) {
        const CSSRule& rule = sheet_->rules[i];
        for (const auto& sel : rule.selectors) {
            if (!sel.tagName.empty() && sel.tagName != "*") {
                tagIndex_.insert({toLower(sel.tagName), i});
            }
            if (!sel.id.empty()) {
                idIndex_.insert({sel.id, i});
            }
            for (const auto& cls : sel.classes) {
                classIndex_.insert({cls, i});
            }
        }
    }
}

std::string RuleMatcher::getClassesAttribute(const html::DOMNode* element) const {
    auto it = element->attributes.find("class");
    if (it == element->attributes.end()) return "";
    return it->second;
}

bool RuleMatcher::tagMatches(const std::string& selectorTag, const html::DOMNode* element) const {
    if (selectorTag == "*") return true;
    return toLower(element->tagName) == toLower(selectorTag);
}

bool RuleMatcher::classMatches(const std::vector<std::string>& selectorClasses, const html::DOMNode* element) const {
    if (selectorClasses.empty()) return true;
    std::string classAttr = getClassesAttribute(element);
    auto elemClasses = split(classAttr);
    for (const auto& sc : selectorClasses) {
        bool found = false;
        for (const auto& ec : elemClasses) {
            if (ec == sc) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

bool RuleMatcher::idMatches(const std::string& selectorId, const html::DOMNode* element) const {
    if (selectorId.empty()) return true;
    auto it = element->attributes.find("id");
    if (it == element->attributes.end()) return false;
    return it->second == selectorId;
}

bool RuleMatcher::matchAttribute(const SimpleSelector::AttrTest& test, const std::string& value) const {
    if (test.op.empty()) {
        return true;
    } else if (test.op == "=") {
        return value == test.value;
    } else if (test.op == "~=") {
        auto parts = split(value);
        return std::find(parts.begin(), parts.end(), test.value) != parts.end();
    } else if (test.op == "|=") {
        return value == test.value || startsWith(value, test.value + "-");
    } else if (test.op == "^=") {
        return startsWith(value, test.value);
    } else if (test.op == "$=") {
        return endsWith(value, test.value);
    } else if (test.op == "*=") {
        return value.find(test.value) != std::string::npos;
    }
    return false;
}

bool RuleMatcher::attrMatches(const std::vector<SimpleSelector::AttrTest>& selectorAttrs, const html::DOMNode* element) const {
    if (selectorAttrs.empty()) return true;
    for (const auto& test : selectorAttrs) {
        auto it = element->attributes.find(test.name);
        if (it == element->attributes.end()) return false;
        if (!matchAttribute(test, it->second)) return false;
    }
    return true;
}

bool RuleMatcher::matchesSelector(const SimpleSelector& sel, const html::DOMNode* element) const {
    if (!tagMatches(sel.tagName, element)) return false;
    if (!idMatches(sel.id, element)) return false;
    if (!classMatches(sel.classes, element)) return false;
    if (!attrMatches(sel.attrs, element)) return false;
    return true;
}

bool RuleMatcher::matchesAnySelector(const CSSRule& rule, const html::DOMNode* element) const {
    for (const auto& sel : rule.selectors) {
        if (matchesSelector(sel, element)) return true;
    }
    return false;
}

std::vector<size_t> RuleMatcher::getCandidates(const html::DOMNode* element) const {
    std::unordered_set<size_t> candidates;

    if (!element->tagName.empty()) {
        auto range = tagIndex_.equal_range(toLower(element->tagName));
        for (auto it = range.first; it != range.second; ++it) {
            candidates.insert(it->second);
        }
    }

    auto idIt = element->attributes.find("id");
    if (idIt != element->attributes.end()) {
        auto range = idIndex_.equal_range(idIt->second);
        for (auto it = range.first; it != range.second; ++it) {
            candidates.insert(it->second);
        }
    }

    std::string classAttr = getClassesAttribute(element);
    auto elemClasses = split(classAttr);
    for (const auto& cls : elemClasses) {
        auto range = classIndex_.equal_range(cls);
        for (auto it = range.first; it != range.second; ++it) {
            candidates.insert(it->second);
        }
    }

    return std::vector<size_t>(candidates.begin(), candidates.end());
}

size_t RuleMatcher::computeSpecificity(const SimpleSelector& sel) const {
    size_t specificity = 0;
    specificity += static_cast<size_t>(sel.id.empty() ? 0 : 1) << 16;
    size_t classCount = sel.classes.size() + sel.attrs.size();
    specificity += static_cast<size_t>(classCount) << 8;
    if (sel.tagName != "*" && !sel.tagName.empty()) {
        specificity += 1;
    }
    return specificity;
}

size_t RuleMatcher::computeRuleSpecificity(const CSSRule& rule) const {
    size_t maxSpec = 0;
    for (const auto& sel : rule.selectors) {
        size_t spec = computeSpecificity(sel);
        if (spec > maxSpec) maxSpec = spec;
    }
    return maxSpec;
}

// ── MediaParser ────────────────────────────────────────────────────────────────

std::vector<ResolvedSheet> MediaParser::expandMediaRules(const std::string& css) {
    std::vector<Cascade::ResolvedMediaSheet> result;

    std::string baseCss;
    std::vector<std::pair<MediaCondition, std::string>> mediaBlocks;

    size_t pos = 0;
    while (pos < css.size()) {
        size_t atPos = css.find("@media", pos);
        if (atPos == std::string::npos) {
            baseCss += css.substr(pos);
            break;
        }

        baseCss += css.substr(pos, atPos - pos);

        size_t condStart = atPos + 6;
        while (condStart < css.size() && std::isspace(static_cast<unsigned char>(css[condStart]))) condStart++;

        size_t braceOpen = css.find('{', condStart);
        if (braceOpen == std::string::npos) {
            baseCss += css.substr(atPos);
            break;
        }

        std::string condText = css.substr(condStart, braceOpen - condStart);
        condText.erase(0, condText.find_first_not_of(" \t\n\r"));
        condText.erase(condText.find_last_not_of(" \t\n\r") + 1);

        MediaCondition cond;
        cond.minWidth = -1;
        cond.maxWidth = INT_MAX;

        size_t mwPos = condText.find("min-width:");
        if (mwPos != std::string::npos) {
            size_t valStart = mwPos + 10;
            std::string val;
            while (valStart < condText.size() && std::isspace(static_cast<unsigned char>(condText[valStart]))) valStart++;
            while (valStart < condText.size() && (std::isdigit(static_cast<unsigned char>(condText[valStart])) || condText[valStart] == '.')) {
                val += condText[valStart++];
            }
            if (!val.empty()) cond.minWidth = static_cast<int>(std::round(std::stof(val)));
        }

        size_t maxwPos = condText.find("max-width:");
        if (maxwPos != std::string::npos) {
            size_t valStart = maxwPos + 10;
            std::string val;
            while (valStart < condText.size() && std::isspace(static_cast<unsigned char>(condText[valStart]))) valStart++;
            while (valStart < condText.size() && (std::isdigit(static_cast<unsigned char>(condText[valStart])) || condText[valStart] == '.')) {
                val += condText[valStart++];
            }
            if (!val.empty()) cond.maxWidth = static_cast<int>(std::round(std::stof(val)));
        }

        int depth = 1;
        size_t contentStart = braceOpen + 1;
        size_t i = contentStart;
        while (i < css.size() && depth > 0) {
            if (css[i] == '{') depth++;
            else if (css[i] == '}') depth--;
            i++;
        }
        std::string innerCss = css.substr(contentStart, i - contentStart - 1);
        mediaBlocks.emplace_back(cond, innerCss);
        pos = i;
    }

    CSSParser parser;

    if (!baseCss.empty()) {
        StyleSheet baseSheet = parser.parse(baseCss);
        if (baseSheet.valid || !baseSheet.rules.empty()) {
            result.push_back(ResolvedSheet{MediaCondition{}, std::move(baseSheet)});
        }
    } else {
        result.push_back(ResolvedSheet{MediaCondition{}, StyleSheet{}});
    }

    for (auto& mb : mediaBlocks) {
        StyleSheet sheet = parser.parse(mb.second);
        if (sheet.valid || !sheet.rules.empty()) {
            result.push_back(ResolvedSheet{std::move(mb.first), std::move(sheet)});
        }
    }

    return result;
}

// ── Cascade ────────────────────────────────────────────────────────────────────

Cascade::Cascade(std::vector<ResolvedMediaSheet> mediaSheets, int viewportWidth)
    : mediaSheets_(std::move(mediaSheets))
    , viewportWidth_(viewportWidth) {}

Cascade::~Cascade() = default;

void Cascade::applyInlineStyles(html::DOMNode* element) {
    auto it = element->attributes.find("style");
    if (it == element->attributes.end()) return;

    CSSParser parser;
    std::string css = "dummy{" + it->second + "}";
    auto sheet = parser.parse(css);
    if (!sheet.rules.empty() && !sheet.rules[0].declarations.empty()) {
        applyDeclarations(element, sheet.rules[0].declarations);
    }
}

void Cascade::applyDeclarations(html::DOMNode* element,
                                const std::vector<std::pair<const char*, const char*>>& declarations) {
    for (const auto& decl : declarations) {
        parseProperty(element, decl.first, decl.second);
    }
}

void Cascade::parseProperty(html::DOMNode* element, const char* prop, const char* val) {
    std::string property = toLower(prop);
    std::string value = val;

    auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
        return s;
    };
    std::string trimmedVal = trim(value);

    if (!element->style) return;

    if (property == "display") {
        element->style->display = parseDisplay(trimmedVal);
    } else if (property == "position") {
        element->style->position = parsePosition(trimmedVal);
    } else if (property == "float" || property == "float_") {
        element->style->float_ = parseFloat(trimmedVal);
    } else if (property == "width") {
        element->style->width = parseLength(trimmedVal, element->style->width);
    } else if (property == "height") {
        element->style->height = parseLength(trimmedVal, element->style->height);
    } else if (property == "min-width") {
        element->style->minWidth = parseLength(trimmedVal, element->style->minWidth);
    } else if (property == "max-width") {
        element->style->maxWidth = parseLength(trimmedVal, element->style->maxWidth);
    } else if (property == "min-height") {
        element->style->minHeight = parseLength(trimmedVal, element->style->minHeight);
    } else if (property == "max-height") {
        element->style->maxHeight = parseLength(trimmedVal, element->style->maxHeight);
    } else if (property == "margin-top") {
        element->style->marginTop = parseLength(trimmedVal, element->style->marginTop);
    } else if (property == "margin-right") {
        element->style->marginRight = parseLength(trimmedVal, element->style->marginRight);
    } else if (property == "margin-bottom") {
        element->style->marginBottom = parseLength(trimmedVal, element->style->marginBottom);
    } else if (property == "margin-left") {
        element->style->marginLeft = parseLength(trimmedVal, element->style->marginLeft);
    } else if (property == "margin") {
        auto parts = split(trimmedVal);
        if (parts.size() == 1) {
            float v = parseLength(parts[0], 0.0f);
            element->style->marginTop = element->style->marginRight = element->style->marginBottom = element->style->marginLeft = v;
        } else if (parts.size() == 2) {
            float v1 = parseLength(parts[0], 0.0f);
            float v2 = parseLength(parts[1], 0.0f);
            element->style->marginTop = element->style->marginBottom = v1;
            element->style->marginRight = element->style->marginLeft = v2;
        } else if (parts.size() == 4) {
            element->style->marginTop = parseLength(parts[0], 0.0f);
            element->style->marginRight = parseLength(parts[1], 0.0f);
            element->style->marginBottom = parseLength(parts[2], 0.0f);
            element->style->marginLeft = parseLength(parts[3], 0.0f);
        }
    } else if (property == "border-top-width") {
        element->style->borderTop = parseLength(trimmedVal, element->style->borderTop);
    } else if (property == "border-right-width") {
        element->style->borderRight = parseLength(trimmedVal, element->style->borderRight);
    } else if (property == "border-bottom-width") {
        element->style->borderBottom = parseLength(trimmedVal, element->style->borderBottom);
    } else if (property == "border-left-width") {
        element->style->borderLeft = parseLength(trimmedVal, element->style->borderLeft);
    } else if (property == "border-width") {
        auto parts = split(trimmedVal);
        if (parts.size() == 1) {
            float v = parseLength(parts[0], 0.0f);
            element->style->borderTop = element->style->borderRight = element->style->borderBottom = element->style->borderLeft = v;
        } else if (parts.size() == 2) {
            float v1 = parseLength(parts[0], 0.0f);
            float v2 = parseLength(parts[1], 0.0f);
            element->style->borderTop = element->style->borderBottom = v1;
            element->style->borderRight = element->style->borderLeft = v2;
        } else if (parts.size() == 4) {
            element->style->borderTop = parseLength(parts[0], 0.0f);
            element->style->borderRight = parseLength(parts[1], 0.0f);
            element->style->borderBottom = parseLength(parts[2], 0.0f);
            element->style->borderLeft = parseLength(parts[3], 0.0f);
        }
    } else if (property == "padding-top") {
        element->style->paddingTop = parseLength(trimmedVal, element->style->paddingTop);
    } else if (property == "padding-right") {
        element->style->paddingRight = parseLength(trimmedVal, element->style->paddingRight);
    } else if (property == "padding-bottom") {
        element->style->paddingBottom = parseLength(trimmedVal, element->style->paddingBottom);
    } else if (property == "padding-left") {
        element->style->paddingLeft = parseLength(trimmedVal, element->style->paddingLeft);
    } else if (property == "padding") {
        auto parts = split(trimmedVal);
        if (parts.size() == 1) {
            float v = parseLength(parts[0], 0.0f);
            element->style->paddingTop = element->style->paddingRight = element->style->paddingBottom = element->style->paddingLeft = v;
        } else if (parts.size() == 2) {
            float v1 = parseLength(parts[0], 0.0f);
            float v2 = parseLength(parts[1], 0.0f);
            element->style->paddingTop = element->style->paddingBottom = v1;
            element->style->paddingRight = element->style->paddingLeft = v2;
        } else if (parts.size() == 4) {
            element->style->paddingTop = parseLength(parts[0], 0.0f);
            element->style->paddingRight = parseLength(parts[1], 0.0f);
            element->style->paddingBottom = parseLength(parts[2], 0.0f);
            element->style->paddingLeft = parseLength(parts[3], 0.0f);
        }
    } else if (property == "font-size") {
        element->style->fontSize = parseLength(trimmedVal, element->style->fontSize);
    } else if (property == "font-family") {
        if (trimmedVal.size() >= 2 && trimmedVal.front() == '"' && trimmedVal.back() == '"') {
            element->style->fontFamily = trimmedVal.substr(1, trimmedVal.size() - 2);
        } else {
            element->style->fontFamily = trimmedVal;
        }
    } else if (property == "font-weight") {
        if (trimmedVal == "normal" || trimmedVal == "400") element->style->fontWeight = 400;
        else if (trimmedVal == "bold" || trimmedVal == "700") element->style->fontWeight = 700;
        else {
            char* end = nullptr;
            int w = std::strtol(trimmedVal.c_str(), &end, 10);
            if (end && *end == '\0') element->style->fontWeight = std::max(100, std::min(900, w));
        }
    } else if (property == "line-height") {
        if (trimmedVal.find("px") != std::string::npos || trimmedVal.find("em") != std::string::npos || std::isdigit(static_cast<unsigned char>(trimmedVal[0]))) {
            element->style->lineHeight = parseLength(trimmedVal, element->style->lineHeight);
        } else {
            char* end = nullptr;
            float factor = std::strtof(trimmedVal.c_str(), &end);
            if (end && *end == '\0' && factor > 0.0f) {
                element->style->lineHeight = factor;
            }
        }
    } else if (property == "letter-spacing") {
        element->style->letterSpacing = parseLength(trimmedVal, element->style->letterSpacing);
    } else if (property == "color") {
        element->style->color = parseColor(trimmedVal);
    } else if (property == "background-color") {
        element->style->backgroundColor = parseColor(trimmedVal);
    } else if (property == "opacity") {
        char* end = nullptr;
        float op = std::strtof(trimmedVal.c_str(), &end);
        if (end && *end == '\0') {
            element->style->opacity = std::max(0.0f, std::min(1.0f, op));
        }
    } else if (property == "box-sizing") {
        element->style->boxSizing = parseBoxSizing(trimmedVal);
    } else if (property == "border-top-left-radius") {
        element->style->borderRadiusTopLeft = parseLength(trimmedVal, element->style->borderRadiusTopLeft);
    } else if (property == "border-top-right-radius") {
        element->style->borderRadiusTopRight = parseLength(trimmedVal, element->style->borderRadiusTopRight);
    } else if (property == "border-bottom-right-radius") {
        element->style->borderRadiusBottomRight = parseLength(trimmedVal, element->style->borderRadiusBottomRight);
    } else if (property == "border-bottom-left-radius") {
        element->style->borderRadiusBottomLeft = parseLength(trimmedVal, element->style->borderRadiusBottomLeft);
    } else if (property == "border-radius") {
        element->style->borderRadiusTopLeft = parseLength(trimmedVal, element->style->borderRadiusTopLeft);
        element->style->borderRadiusTopRight = element->style->borderRadiusTopLeft;
        element->style->borderRadiusBottomRight = element->style->borderRadiusTopLeft;
        element->style->borderRadiusBottomLeft = element->style->borderRadiusTopLeft;
    } else if (property == "overflow") {
        element->style->overflow = parseOverflow(trimmedVal);
    }
}

float Cascade::parseLength(const std::string& val, float base) {
    std::string v = val;
    v.erase(0, v.find_first_not_of(" \t\n\r"));
    v.erase(v.find_last_not_of(" \t\n\r") + 1);

    if (v == "auto") return base;
    if (v == "0") return 0.0f;

    if (endsWith(v, "px")) {
        std::string num = v.substr(0, v.size() - 2);
        char* end = nullptr;
        float f = std::strtof(num.c_str(), &end);
        if (end && *end == '\0') return f;
    } else if (endsWith(v, "em")) {
        std::string num = v.substr(0, v.size() - 2);
        char* end = nullptr;
        float f = std::strtof(num.c_str(), &end);
        if (end && *end == '\0') return f * 16.0f;
    } else if (endsWith(v, "rem")) {
        std::string num = v.substr(0, v.size() - 3);
        char* end = nullptr;
        float f = std::strtof(num.c_str(), &end);
        if (end && *end == '\0') return f * 16.0f;
    } else if (endsWith(v, "%")) {
        std::string num = v.substr(0, v.size() - 1);
        char* end = nullptr;
        float f = std::strtof(num.c_str(), &end);
        if (end && *end == '\0') return f;
    } else {
        char* end = nullptr;
        float f = std::strtof(v.c_str(), &end);
        if (end && *end == '\0') return f;
    }
    return base;
}

uint32_t Cascade::parseColor(const std::string& val) {
    std::string v = toLower(val);
    v.erase(0, v.find_first_not_of(" \t\n\r"));
    v.erase(v.find_last_not_of(" \t\n\r") + 1);

    static const std::map<std::string, uint32_t> namedColors = {
        {"black",    0xFF000000u},
        {"white",    0xFFFFFFFFu},
        {"red",      0xFFFF0000u},
        {"green",    0xFF00FF00u},
        {"blue",     0xFF0000FFu},
        {"yellow",   0xFFFFFF00u},
        {"cyan",     0xFF00FFFFu},
        {"magenta",  0xFFFF00FFu},
        {"gray",     0xFF808080u},
        {"grey",     0xFF808080u},
        {"orange",   0xFFFFA500u},
        {"purple",   0xFF800080u},
        {"pink",     0xFFFFC0CBu},
        {"brown",    0xFFA52A2Au},
        {"transparent", 0x00000000u},
    };

    auto it = namedColors.find(v);
    if (it != namedColors.end()) return it->second;

    if (startsWith(v, "#")) {
        std::string hex = v.substr(1);
        if (hex.size() == 3) {
            int r = std::stoi(hex.substr(0, 1) + hex.substr(0, 1), nullptr, 16);
            int g = std::stoi(hex.substr(1, 1) + hex.substr(1, 1), nullptr, 16);
            int b = std::stoi(hex.substr(2, 1) + hex.substr(2, 1), nullptr, 16);
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        } else if (hex.size() == 6) {
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        } else if (hex.size() == 8) {
            int a = std::stoi(hex.substr(0, 2), nullptr, 16);
            int r = std::stoi(hex.substr(2, 2), nullptr, 16);
            int g = std::stoi(hex.substr(4, 2), nullptr, 16);
            int b = std::stoi(hex.substr(6, 2), nullptr, 16);
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    if (startsWith(v, "rgb(")) {
        size_t end = v.find(')', 4);
        std::string inside = v.substr(4, end - 4);
        auto parts = split(inside);
        if (parts.size() == 3) {
            int r = std::stoi(parts[0]);
            int g = std::stoi(parts[1]);
            int b = std::stoi(parts[2]);
            return 0xFF000000u | (std::clamp(r, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(b, 0, 255);
        }
    }

    if (startsWith(v, "rgba(")) {
        size_t end = v.find(')', 5);
        std::string inside = v.substr(5, end - 5);
        auto parts = split(inside);
        if (parts.size() == 4) {
            int r = std::stoi(parts[0]);
            int g = std::stoi(parts[1]);
            int b = std::stoi(parts[2]);
            float a = std::clamp(std::stof(parts[3]), 0.0f, 1.0f);
            int alpha = static_cast<int>(a * 255.0f);
            return (alpha << 24) | (std::clamp(r, 0, 255) << 16) | (std::clamp(g, 0, 255) << 8) | std::clamp(b, 0, 255);
        }
    }

    return 0xFF000000u;
}

ComputedStyle::Display Cascade::parseDisplay(const std::string& val) {
    std::string v = toLower(val);
    v.erase(0, v.find_first_not_of(" \t\n\r"));
    v.erase(v.find_last_not_of(" \t\n\r") + 1);

    if (v == "none") return ComputedStyle::None;
    if (v == "block") return ComputedStyle::Block;
    if (v == "inline") return ComputedStyle::Inline;
    if (v == "inline-block") return ComputedStyle::InlineBlock;
    if (v == "flex") return ComputedStyle::Flex;
    if (v == "grid") return ComputedStyle::Grid;
    if (v == "table") return ComputedStyle::Table;
    if (v == "table-row") return ComputedStyle::TableRow;
    if (v == "table-cell") return ComputedStyle::TableCell;
    return ComputedStyle::Block;
}

ComputedStyle::Position Cascade::parsePosition(const std::string& val) {
    std::string v = toLower(val);
    if (v == "static") return ComputedStyle::Static;
    if (v == "relative") return ComputedStyle::Relative;
    if (v == "absolute") return ComputedStyle::Absolute;
    if (v == "fixed") return ComputedStyle::Fixed;
    if (v == "sticky") return ComputedStyle::Sticky;
    return ComputedStyle::Static;
}

ComputedStyle::Float Cascade::parseFloat(const std::string& val) {
    std::string v = toLower(val);
    if (v == "none" || v == "0") return ComputedStyle::FNone;
    if (v == "left") return ComputedStyle::Left;
    if (v == "right") return ComputedStyle::Right;
    return ComputedStyle::FNone;
}

ComputedStyle::BoxSizing Cascade::parseBoxSizing(const std::string& val) {
    std::string v = toLower(val);
    if (v == "border-box") return ComputedStyle::BorderBox;
    return ComputedStyle::ContentBox;
}

ComputedStyle::Overflow Cascade::parseOverflow(const std::string& val) {
    std::string v = toLower(val);
    if (v == "hidden") return ComputedStyle::Hidden;
    if (v == "scroll") return ComputedStyle::Scroll;
    if (v == "auto") return ComputedStyle::Auto;
    return ComputedStyle::Visible;
}

std::vector<std::string> Cascade::splitClasses(const std::string& classAttr) {
    return split(classAttr);
}

void Cascade::applyCascadeToElement(html::DOMNode* element) {
    if (!element) return;

    if (!element->style) {
        element->style = std::make_unique<ComputedStyle>();
    } else {
        *element->style = ComputedStyle{};
    }

    for (const auto& rms : mediaSheets_) {
        if (!rms.condition.matches(viewportWidth_)) continue;
        if (rms.sheet.rules.empty()) continue;

        RuleMatcher matcher(rms.sheet);
        matcher.buildIndex();

        std::vector<std::pair<size_t, size_t>> matched;
        matched.reserve(rms.sheet.rules.size());

        for (size_t i = 0; i < rms.sheet.rules.size(); ++i) {
            if (matcher.matchesAnySelector(rms.sheet.rules[i], element)) {
                matched.emplace_back(i, matcher.computeRuleSpecificity(rms.sheet.rules[i]));
            }
        }

        std::sort(matched.begin(), matched.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });

        for (const auto& m : matched) {
            applyDeclarations(element, rms.sheet.rules[m.first].declarations);
        }
    }

    applyInlineStyles(element);
}

void Cascade::resolve(html::DOMNode* document) {
    if (!document) return;

    std::function<void(html::DOMNode*)> walk;
    walk = [this, &walk](html::DOMNode* node) {
        if (!node) return;
        applyCascadeToElement(node);
        for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
            walk(child);
        }
    };

    walk(document);
}

} // namespace css
