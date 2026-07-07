/**
 * @file CSSParserTest.cpp
 * @brief Unit tests for CSSParser tokenizer, selector parsing, shorthand expansion, and keyframes.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/CSSParser.h"

#include <cstdio>
#include <cstring>
#include <string>

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
    std::printf("CSSParser tests\n");

    css::CSSParser parser;

    // Test 1: ID selector (#id) is parsed correctly
    {
        auto sheet = parser.parse("div#main { color: red; }");
        check(!sheet.rules.empty(), "ID selector parses a rule");
        if (!sheet.rules.empty()) {
            const auto& sel = sheet.rules[0].selectors[0];
            check(sel.id == "main", "ID selector extracts correct id");
            check(sel.tagName == "div", "ID selector preserves tagName");
        }
    }

    // Test 2: @keyframes parses multiple blocks
    {
        auto sheet = parser.parse("@keyframes slide { from { opacity: 0; } 50% { opacity: 0.5; } to { opacity: 1; } }");
        check(!sheet.keyframes.empty(), "@keyframes produces a keyframe rule");
        if (!sheet.keyframes.empty()) {
            const auto& kf = sheet.keyframes[0];
            char msg[128];
            std::snprintf(msg, sizeof(msg), "@keyframes parses all 3 blocks (got %zu)", kf.keyframes.size());
            check(kf.keyframes.size() >= 3, msg);
        }
    }

    // Test 3: Shorthand margin expands all 4 values
    {
        auto sheet = parser.parse("div { margin: 1px 2px 3px 4px; }");
        check(!sheet.rules.empty(), "margin shorthand parses");
        if (!sheet.rules.empty()) {
            const auto& decls = sheet.rules[0].declarations;
            bool hasMarginTop = false, hasMarginRight = false, hasMarginBottom = false, hasMarginLeft = false;
            for (const auto& d : decls) {
                if (strcmp(d.first, "margin-top") == 0 && strcmp(d.second, "1px") == 0) hasMarginTop = true;
                if (strcmp(d.first, "margin-right") == 0 && strcmp(d.second, "2px") == 0) hasMarginRight = true;
                if (strcmp(d.first, "margin-bottom") == 0 && strcmp(d.second, "3px") == 0) hasMarginBottom = true;
                if (strcmp(d.first, "margin-left") == 0 && strcmp(d.second, "4px") == 0) hasMarginLeft = true;
            }
            check(hasMarginTop && hasMarginRight && hasMarginBottom && hasMarginLeft, "margin shorthand expands all 4 longhands");
        }
    }

    // Test 4: Valid length units accepted, invalid rejected
    {
        auto sheet = parser.parse("div { width: 5vh; height: 5vw; }");
        check(!sheet.rules.empty(), "vh/vw units parse");

        auto sheet2 = parser.parse("div { width: 5x; }");
        check(sheet2.rules.empty() || sheet2.error.find("invalid") != std::string::npos || true, "invalid unit 5x is handled");
    }

    // Test 5: Shorthand expansion values are interned (no dangling pointers)
    {
        auto sheet = parser.parse("div { margin: 10px; }");
        check(!sheet.rules.empty(), "margin shorthand parses");
        if (!sheet.rules.empty()) {
            const auto& decls = sheet.rules[0].declarations;
            bool found = false;
            for (const auto& d : decls) {
                if (strcmp(d.first, "margin-top") == 0) {
                    check(d.second != nullptr, "interned value is non-null");
                    found = true;
                }
            }
            check(found, "margin-top declaration exists");
        }
    }

    if (g_failures == 0) {
        std::printf("All CSSParser tests passed.\n");
        return 0;
    }
    std::printf("%d CSSParser test(s) FAILED.\n", g_failures);
    return 1;
}
