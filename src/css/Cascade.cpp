/**
 * @file css/Cascade.cpp
 * @brief CSS cascade engine implementation (Bead B).
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/Cascade.h"

#include "css/CSSParser.h"
#include "html/DOMNode.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace css {

namespace {

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

// One simple selector inside a compound: tag / #id / .class / :pseudo.
struct SimpleSel {
    std::string tag;            ///< "*", "" (none) or a tag name.
    std::string id;             ///< #id value, empty if absent.
    std::vector<std::string> classes;   ///< .class tokens.
    std::vector<std::string> pseudos;   ///< :visited / :link / :hover (no-ops).
};

// A compound selector: a chain of simple selectors with no combinator.
struct Compound {
    std::vector<SimpleSel> simples;
};

enum class Combinator { Descendant, Child };

// A complex selector: compounds joined by combinators, right to left.
struct ComplexSel {
    Combinator comb{Combinator::Descendant};
    Compound compound;
};

// Parse a single simple selector starting at i; advances i past it.
// Returns false if nothing parseable was found.
bool parseSimple(const std::string& s, size_t& i, SimpleSel& out) {
    out = SimpleSel{};
    const size_t n = s.size();
    if (i >= n) return false;
    char ch = s[i];
    if (ch == '#') {
        ++i;
        std::string v;
        while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
               s[i] != '>' && s[i] != '#' && s[i] != '.' && s[i] != ':')
            v += s[i++];
        out.id = v;
        return true;
    }
    if (ch == '.') {
        ++i;
        std::string v;
        while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
               s[i] != '>' && s[i] != '#' && s[i] != '.' && s[i] != ':')
            v += s[i++];
        out.classes.push_back(v);
        return true;
    }
    if (ch == ':') {
        ++i;
        std::string v;
        while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
               s[i] != '>' && s[i] != '#' && s[i] != '.' && s[i] != ':')
            v += s[i++];
        out.pseudos.push_back(v);
        return true;
    }
    if (ch == '*') {
        out.tag = "*";
        ++i;
        return true;
    }
    // type selector: identifier
    std::string v;
    while (i < n && !std::isspace(static_cast<unsigned char>(s[i])) &&
           s[i] != '>' && s[i] != '#' && s[i] != '.' && s[i] != ':')
        v += s[i++];
    if (v.empty()) return false;
    out.tag = v;
    return true;
}

// Parse a compound selector (sequence of simples with no combinator).
bool parseCompound(const std::string& s, size_t& i, Compound& out) {
    out.simples.clear();
    bool any = false;
    while (i < s.size()) {
        char ch = s[i];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '>') break;
        SimpleSel sm;
        if (!parseSimple(s, i, sm)) break;
        out.simples.push_back(sm);
        any = true;
    }
    return any;
}

// Parse a complex selector (compounds joined by descendant / child combinators).
// Returns the list in document order (left -> right) and sets the combinator for
// every compound except the first (which is Descendant by definition).
std::vector<ComplexSel> parseComplex(const std::string& sel) {
    std::vector<ComplexSel> out;
    size_t i = 0;
    const size_t n = sel.size();
    while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) ++i;
    if (i >= n) return out;

    Compound first;
    if (!parseCompound(sel, i, first)) return out;
    out.push_back(ComplexSel{Combinator::Descendant, first});

    while (i < n) {
        Combinator comb = Combinator::Descendant;
        while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) {
            comb = Combinator::Descendant;
            ++i;
        }
        if (i < n && sel[i] == '>') {
            comb = Combinator::Child;
            ++i;
            while (i < n && std::isspace(static_cast<unsigned char>(sel[i]))) ++i;
        }
        Compound c;
        if (!parseCompound(sel, i, c)) break;
        out.push_back(ComplexSel{comb, c});
    }
    return out;
}

// Does a single simple selector match the element (ignoring combinators)?
bool matchSimple(const html::DOMNode* node, const SimpleSel& sm) {
    if (node->nodeType != html::NodeType::Element) return false;
    if (!sm.tag.empty() && sm.tag != "*" && !iequals(node->tagName, sm.tag))
        return false;
    if (!sm.id.empty()) {
        auto it = node->attributes.find("id");
        if (it == node->attributes.end() || it->second != sm.id) return false;
    }
    for (const auto& cls : sm.classes) {
        auto it = node->attributes.find("class");
        if (it == node->attributes.end()) return false;
        bool found = false;
        std::istringstream ss(it->second);
        std::string tok;
        while (ss >> tok) {
            if (tok == cls) { found = true; break; }
        }
        if (!found) return false;
    }
    // Pseudo-classes (:visited / :link / :hover) are no-ops for now.
    return true;
}

bool matchCompound(const html::DOMNode* node, const Compound& c) {
    for (const auto& sm : c.simples) {
        if (!matchSimple(node, sm)) return false;
    }
    return true;
}

// Compute specificity for one complex selector (no element needed).
Specificity specificityOfComplex(const std::vector<ComplexSel>& parts) {
    Specificity s;
    for (const auto& part : parts) {
        for (const auto& sm : part.compound.simples) {
            if (!sm.tag.empty() && sm.tag != "*") s.typeCount += 1;
            else if (!sm.tag.empty() && sm.tag == "*") s.typeCount += 1;
            if (!sm.id.empty()) s.idCount += 1;
            s.classCount += static_cast<int>(sm.classes.size());
            s.classCount += static_cast<int>(sm.pseudos.size());
        }
    }
    return s;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

bool Cascade::matches(const html::DOMNode* element, const std::string& selector) {
    if (element == nullptr) return false;
    std::vector<ComplexSel> parts = parseComplex(selector);
    if (parts.empty()) return false;

    // Match right-to-left.  The last (rightmost) compound must match `element`.
    size_t last = parts.size() - 1;
    if (!matchCompound(element, parts[last].compound)) return false;

    const html::DOMNode* cur = element;  // the node matched by the right side
    // Walk leftwards.  The combinator that governs the relationship between the
    // compound at `k` and the already-matched node `cur` is the combinator
    // stored on the compound to the right of `k` (parts[k+1].comb).
    for (long k = static_cast<long>(last) - 1; k >= 0; --k) {
        Combinator comb = parts[k + 1].comb;

        if (comb == Combinator::Child) {
            // `cur`'s immediate parent must match parts[k].
            const html::DOMNode* parent = cur->parent;
            if (parent == nullptr || !matchCompound(parent, parts[k].compound))
                return false;
            cur = parent;
        } else {  // Descendant: any ancestor (nearest first) must match.
            bool found = false;
            for (const html::DOMNode* p = cur->parent; p != nullptr; p = p->parent) {
                if (matchCompound(p, parts[k].compound)) {
                    cur = p;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }
    return true;
}

Specificity Cascade::specificityOf(const std::string& selector) {
    return specificityOfComplex(parseComplex(selector));
}

std::map<std::string, std::string>
Cascade::computeStyle(const html::DOMNode* element, const std::vector<CSSRule>& rules) {
    std::map<std::string, std::string> result;
    if (element == nullptr) return result;

    std::vector<MatchedRule> matched;

    // Author rules in source order.
    for (size_t i = 0; i < rules.size(); ++i) {
        const CSSRule& r = rules[i];
        if (r.type != RuleType::Style) continue;

        // A rule may carry a comma-separated list of selectors.
        // Split on top-level commas.
        std::vector<std::string> selectors;
        std::string cur;
        for (char ch : r.selector) {
            if (ch == ',') {
                // strip surrounding whitespace
                size_t a = cur.find_first_not_of(" \t\r\n\f");
                size_t z = cur.find_last_not_of(" \t\r\n\f");
                if (a != std::string::npos) selectors.push_back(cur.substr(a, z - a + 1));
                cur.clear();
            } else {
                cur += ch;
            }
        }
        size_t a = cur.find_first_not_of(" \t\r\n\f");
        size_t z = cur.find_last_not_of(" \t\r\n\f");
        if (a != std::string::npos) selectors.push_back(cur.substr(a, z - a + 1));

        bool anyMatch = false;
        Specificity bestSpec;
        for (const auto& sel : selectors) {
            if (matches(element, sel)) {
                anyMatch = true;
                Specificity sp = specificityOfComplex(parseComplex(sel));
                if (bestSpec < sp) bestSpec = sp;
            }
        }
        if (!anyMatch) continue;
        bestSpec.sourceOrder = static_cast<int>(i);
        MatchedRule mr;
        mr.rule = &r;
        mr.declarations = r.declarations;
        mr.specificity = bestSpec;
        matched.push_back(std::move(mr));
    }

    // Inline style: highest specificity (inlineCount = 1), beats all author rules.
    auto inlineIt = element->attributes.find("style");
    if (inlineIt != element->attributes.end()) {
        auto inlineDecls = CSSParser::parseInlineStyle(inlineIt->second);
        if (!inlineDecls.empty()) {
            Specificity sp;
            sp.inlineCount = 1;
            sp.sourceOrder = static_cast<int>(rules.size());  // after all author
            MatchedRule mr;
            mr.rule = nullptr;
            mr.declarations = std::move(inlineDecls);
            mr.specificity = sp;
            matched.push_back(std::move(mr));
        }
    }

    // Order by ascending specificity/source; later entries win on conflict.
    std::sort(matched.begin(), matched.end());
    for (const auto& mr : matched) {
        for (const auto& kv : mr.declarations) {
            result[kv.first] = kv.second;  // later (higher spec) overwrites.
        }
    }
    return result;
}

ComputedStyle Cascade::computeStyle(const html::DOMNode* element, const html::Document* doc) {
    ComputedStyle result;
    if (!element || !doc) return result;

    std::map<std::string, std::string> decls;
    for (const auto& rule : doc->stylesheets) {
        if (rule.type != RuleType::Style) continue;
        std::vector<std::string> selectors;
        std::string cur;
        for (char ch : rule.selector) {
            if (ch == ',') {
                size_t a = cur.find_first_not_of(" \t\r\n\f");
                size_t z = cur.find_last_not_of(" \t\r\n\f");
                if (a != std::string::npos) selectors.push_back(cur.substr(a, z - a + 1));
                cur.clear();
            } else {
                cur += ch;
            }
        }
        size_t a = cur.find_first_not_of(" \t\r\n\f");
        size_t z = cur.find_last_not_of(" \t\r\n\f");
        if (a != std::string::npos) selectors.push_back(cur.substr(a, z - a + 1));

        bool anyMatch = false;
        Specificity bestSpec;
        for (const auto& sel : selectors) {
            if (matches(element, sel)) {
                anyMatch = true;
                Specificity sp = specificityOfComplex(parseComplex(sel));
                if (bestSpec < sp) bestSpec = sp;
            }
        }
        if (!anyMatch) continue;
        bestSpec.sourceOrder = static_cast<int>(&rule - &doc->stylesheets[0]);
        for (const auto& kv : rule.declarations) {
            decls[kv.first] = kv.second;
        }
    }

    auto inlineIt = element->attributes.find("style");
    if (inlineIt != element->attributes.end()) {
        auto inlineDecls = CSSParser::parseInlineStyle(inlineIt->second);
        for (const auto& kv : inlineDecls) {
            decls[kv.first] = kv.second;
        }
    }

    auto parseFloat = [](const std::string& s, float def) -> float {
        if (s.empty()) return def;
        const char* end = nullptr;
        float v = std::strtof(s.c_str(), const_cast<char**>(&end));
        if (end == s.c_str()) return def;
        return v;
    };

    auto expandShorthand = [&](const std::string& prop, const std::string& val,
                                std::string& a, std::string& b, std::string& c, std::string& d) {
        if (decls.count(prop + "-top")) return;
        std::vector<std::string> parts;
        std::string cur;
        for (char ch : val) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else {
                cur += ch;
            }
        }
        if (!cur.empty()) parts.push_back(cur);
        if (parts.empty()) return;
        if (parts.size() == 1) {
            a = b = c = d = parts[0];
        } else if (parts.size() == 2) {
            a = c = parts[0];
            b = d = parts[1];
        } else if (parts.size() == 3) {
            a = parts[0];
            b = d = parts[1];
            c = parts[2];
        } else if (parts.size() >= 4) {
            a = parts[0];
            b = parts[1];
            c = parts[2];
            d = parts[3];
        }
        decls[prop + "-top"] = a;
        decls[prop + "-right"] = b;
        decls[prop + "-bottom"] = c;
        decls[prop + "-left"] = d;
    };

    std::string pT, pR, pB, pL;
    expandShorthand("padding", decls.count("padding") ? decls["padding"] : "", pT, pR, pB, pL);
    std::string mT, mR, mB, mL;
    expandShorthand("margin", decls.count("margin") ? decls["margin"] : "", mT, mR, mB, mL);

    auto parseColor = [](const std::string& s) -> uint32_t {
        if (s.empty()) return 0xFF000000;
        if (s[0] == '#') {
            unsigned int hex = 0;
            if (s.size() == 4) {
                hex = (std::stoi(std::string(1, s[1]) + std::string(1, s[1]), nullptr, 16) << 16) |
                      (std::stoi(std::string(1, s[2]) + std::string(1, s[2]), nullptr, 16) << 8)  |
                       std::stoi(std::string(1, s[3]) + std::string(1, s[3]), nullptr, 16);
            } else if (s.size() == 7) {
                hex = std::stoi(s.substr(1), nullptr, 16);
            } else if (s.size() == 9) {
                hex = std::stoul(s.substr(1), nullptr, 16);
            }
            return (hex & 0x00FFFFFF) | 0xFF000000;
        }
        if (s.rfind("rgb(", 0) == 0) {
            int r = 0, g = 0, b = 0;
            if (std::sscanf(s.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3) {
                return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF) | 0xFF000000;
            }
        }
        if (s == "red")    return 0xFFFF0000;
        if (s == "green")  return 0xFF00FF00;
        if (s == "blue")   return 0xFF0000FF;
        if (s == "black")  return 0xFF000000;
        if (s == "white")  return 0xFFFFFFFF;
        if (s == "yellow") return 0xFFFFFF00;
        if (s == "cyan")   return 0xFF00FFFF;
        if (s == "magenta")return 0xFFFF00FF;
        if (s == "orange") return 0xFFFFA500;
        if (s == "purple") return 0xFF800080;
        if (s == "navy")   return 0xFF000080;
        if (s == "teal")   return 0xFF008080;
        if (s == "maroon") return 0xFF800000;
        if (s == "olive")  return 0xFF808000;
        if (s == "gray" || s == "grey") return 0xFF808080;
        return 0xFF000000;
    };

    for (const auto& kv : decls) {
        const std::string& p = kv.first;
        const std::string& v = kv.second;
        if (p == "font-size")       result.fontSize = parseFloat(v, result.fontSize);
        else if (p == "color")      result.color = parseColor(v);
        else if (p == "background-color") result.backgroundColor = parseColor(v);
        else if (p == "border-color")     result.borderColor = parseColor(v);
        else if (p == "margin-top")    result.marginTop = parseFloat(v, result.marginTop);
        else if (p == "margin-right")  result.marginRight = parseFloat(v, result.marginRight);
        else if (p == "margin-bottom") result.marginBottom = parseFloat(v, result.marginBottom);
        else if (p == "margin-left")   result.marginLeft = parseFloat(v, result.marginLeft);
        else if (p == "padding-top")    result.paddingTop = parseFloat(v, result.paddingTop);
        else if (p == "padding-right")  result.paddingRight = parseFloat(v, result.paddingRight);
        else if (p == "padding-bottom") result.paddingBottom = parseFloat(v, result.paddingBottom);
        else if (p == "padding-left")   result.paddingLeft = parseFloat(v, result.paddingLeft);
        else if (p == "border-top-width")    result.borderTop = parseFloat(v, result.borderTop);
        else if (p == "border-right-width")  result.borderRight = parseFloat(v, result.borderRight);
        else if (p == "border-bottom-width") result.borderBottom = parseFloat(v, result.borderBottom);
        else if (p == "border-left-width")   result.borderLeft = parseFloat(v, result.borderLeft);
        else if (p == "display") {
            if (v == "none")            result.display = ComputedStyle::None;
            else if (v == "block")      result.display = ComputedStyle::Block;
            else if (v == "inline")     result.display = ComputedStyle::Inline;
            else if (v == "inline-block") result.display = ComputedStyle::InlineBlock;
            else if (v == "flex")       result.display = ComputedStyle::Flex;
            else if (v == "grid")       result.display = ComputedStyle::Grid;
            else if (v == "table")      result.display = ComputedStyle::Table;
            else if (v == "table-row")  result.display = ComputedStyle::TableRow;
            else if (v == "table-cell") result.display = ComputedStyle::TableCell;
        }
        else if (p == "position") {
            if (v == "static")    result.position = ComputedStyle::Static;
            else if (v == "relative") result.position = ComputedStyle::Relative;
            else if (v == "absolute") result.position = ComputedStyle::Absolute;
            else if (v == "fixed")    result.position = ComputedStyle::Fixed;
            else if (v == "sticky")   result.position = ComputedStyle::Sticky;
        }
        else if (p == "width")  result.width = parseFloat(v, result.width);
        else if (p == "height") result.height = parseFloat(v, result.height);
        else if (p == "font-family") result.fontFamily = v;
        else if (p == "font-weight") {
            int w = std::atoi(v.c_str());
            if (w > 0) result.fontWeight = w;
        }
        else if (p == "text-align") {
            if (v == "center")       result.textAlign = ComputedStyle::AlignCenter;
            else if (v == "right")   result.textAlign = ComputedStyle::AlignRight;
            else                     result.textAlign = ComputedStyle::AlignLeft;
        }
    }

    return result;
}

} // namespace css
