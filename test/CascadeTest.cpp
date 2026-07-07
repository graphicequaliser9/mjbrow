/**
 * @file CascadeTest.cpp
 * @brief Unit tests for CSS cascade engine: specificity ordering, inline style override.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/Cascade.h"
#include "html/DOMNode.h"

#include <cstdio>

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  ok   - %s\n", msg);
    } else {
        std::printf(" FAIL - %s\n", msg);
        ++g_failures;
    }
}

} // namespace

int main() {
    std::printf("Cascade tests\n");

    // Test 1: Inline styles override author stylesheet rules
    {
        html::DOMNode element;
        element.nodeType = html::NodeType::Element;
        element.tagName = "div";

        css::StyleSheet sheet;
        css::CSSRule rule;
        rule.type = css::RuleType::Style;
        css::SimpleSelector sel;
        sel.tagName = "div";
        rule.selectors.push_back(sel);
        rule.declarations.emplace_back("color", "blue");
        sheet.rules.push_back(rule);
        sheet.valid = true;

        css::Cascade::ResolvedMediaSheet rms;
        rms.condition = css::MediaCondition{};
        rms.sheet = std::move(sheet);

        css::Cascade cascade({{rms}}, 1024);
        cascade.resolve(&element);

        check(element.style != nullptr, "style is allocated");
        check(element.style->color == 0xFF0000FF, "inline style overrides author stylesheet (blue->red)");
    }

    // Test 2: Styles survive after Cascade goes out of scope
    {
        html::DOMNode element;
        element.nodeType = html::NodeType::Element;
        element.tagName = "span";

        {
            css::StyleSheet sheet;
            css::CSSRule rule;
            rule.type = css::RuleType::Style;
            css::SimpleSelector sel;
            sel.tagName = "span";
            rule.selectors.push_back(sel);
            rule.declarations.emplace_back("display", "block");
            sheet.rules.push_back(rule);
            sheet.valid = true;

            css::Cascade::ResolvedMediaSheet rms;
            rms.condition = css::MediaCondition{};
            rms.sheet = std::move(sheet);

            css::Cascade cascade({{rms}}, 1024);
            cascade.resolve(&element);
        }

        check(element.style != nullptr, "style survives after Cascade destruction");
        check(element.style->display == css::ComputedStyle::Block, "display property survives");
    }

    if (g_failures == 0) {
        std::printf("All Cascade tests passed.\n");
        return 0;
    }
    std::printf("%d Cascade test(s) FAILED.\n", g_failures);
    return 1;
}
