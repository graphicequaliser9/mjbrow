/**
 * @file tests/CSSTest.cpp
 * @brief Unit tests for the CSS parser (Bead A).
 * @details Exercises the tokenizer, the rule-set parser (type / class / id /
 *          descendant / child selectors), declaration parsing, at-rules
 *          (@media, @font-face), functional values (rgb(), url()) and inline
 *          style="" attribute parsing.  Cascade / inheritance is out of scope.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/CSSParser.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace css;

// ── tiny test harness ────────────────────────────────────────────────────────

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "FAIL: " #cond "  (" << __FILE__ << ":" << __LINE__ \
                      << ")\n";                                             \
        }                                                                    \
    } while (0)

// Find the first style rule with the given selector text.
static const CSSRule* findRule(const std::vector<CSSRule>& rules, const std::string& sel) {
    for (const auto& r : rules) {
        if (r.type == RuleType::Style && r.selector == sel) return &r;
    }
    return nullptr;
}

// ── tokenizer tests ──────────────────────────────────────────────────────────

static void testTokenizerBasics() {
    auto toks = CSSParser::tokenize("div { color: #ff0000; width: 24px; }");

    // First meaningful token is the type selector identifier "div".
    CHECK(toks.front().type == CSSTokenType::Ident);
    CHECK(toks.front().text == "div");

    bool sawBrace = false, sawColor = false, sawHash = false, sawDim = false;
    for (const auto& t : toks) {
        if (t.type == CSSTokenType::LBrace) sawBrace = true;
        if (t.type == CSSTokenType::Ident && t.text == "color") sawColor = true;
        if (t.type == CSSTokenType::Hash && t.text == "ff0000") sawHash = true;
        if (t.type == CSSTokenType::Dimension && t.unit == "px" && t.number == 24.0)
            sawDim = true;
    }
    CHECK(sawBrace);
    CHECK(sawColor);
    CHECK(sawHash);
    CHECK(sawDim);
    CHECK(toks.back().type == CSSTokenType::EOF_TOKEN);
}

static void testTokenizerNumbersAndStrings() {
    auto toks = CSSParser::tokenize("content: \"hello world\"; opacity: 0.5; z-index: -3;");

    bool sawString = false, sawFloat = false, sawNeg = false;
    for (const auto& t : toks) {
        if (t.type == CSSTokenType::String && t.text == "hello world") sawString = true;
        if (t.type == CSSTokenType::Number && t.number == 0.5) sawFloat = true;
        if (t.type == CSSTokenType::Number && t.number == -3.0) sawNeg = true;
    }
    CHECK(sawString);
    CHECK(sawFloat);
    CHECK(sawNeg);
}

static void testTokenizerFunctionsAndUrls() {
    auto toks = CSSParser::tokenize(
        "background: url(\"img/bg.png\") rgb(255, 0, 0);");

    bool sawUrl = false, sawRgb = false;
    for (const auto& t : toks) {
        if (t.type == CSSTokenType::Url && t.text == "img/bg.png") sawUrl = true;
        if (t.type == CSSTokenType::Function && t.text == "rgb") {
            sawRgb = true;
            CHECK(t.unit.find("255") != std::string::npos);
        }
    }
    CHECK(sawUrl);
    CHECK(sawRgb);
}

// ── rule-set parser tests ────────────────────────────────────────────────────

static void testParseSimpleRules() {
    CSSParser parser;
    auto rules = parser.parse(
        "body { margin: 0; background-color: #ffffff; }\n"
        "h1 { font-size: 32px; color: rgb(0, 0, 0); }\n");

    CHECK(rules.size() == 2);

    const CSSRule* body = findRule(rules, "body");
    CHECK(body != nullptr);
    CHECK(body->declarations.size() == 2);
    CHECK(body->declarations.at("margin") == "0");
    CHECK(body->declarations.at("background-color") == "#ffffff");

    const CSSRule* h1 = findRule(rules, "h1");
    CHECK(h1 != nullptr);
    CHECK(h1->declarations.at("font-size") == "32px");
    CHECK(h1->declarations.at("color") == "rgb(0, 0, 0)");
}

static void testParseSelectorKinds() {
    CSSParser parser;
    auto rules = parser.parse(
        ".btn { color: blue; }\n"
        "#main { width: 100%; }\n"
        "div p { line-height: 1.5; }\n"
        "ul > li { list-style: none; }\n");

    CHECK(rules.size() == 4);

    // Class selector.
    const CSSRule* btn = findRule(rules, ".btn");
    CHECK(btn != nullptr);
    CHECK(btn->declarations.at("color") == "blue");

    // Id selector.
    const CSSRule* main = findRule(rules, "#main");
    CHECK(main != nullptr);
    CHECK(main->declarations.at("width") == "100%");

    // Descendant combinator.
    const CSSRule* desc = findRule(rules, "div p");
    CHECK(desc != nullptr);
    CHECK(desc->declarations.at("line-height") == "1.5");

    // Child combinator.
    const CSSRule* child = findRule(rules, "ul > li");
    CHECK(child != nullptr);
    CHECK(child->declarations.at("list-style") == "none");
}

static void testParseCompoundSelector() {
    CSSParser parser;
    auto rules = parser.parse("div.box#inner { display: block; }");
    CHECK(rules.size() == 1);
    CHECK(rules[0].selector == "div.box#inner");
    CHECK(rules[0].declarations.at("display") == "block");
}

static void testParseComments() {
    CSSParser parser;
    auto rules = parser.parse(
        "/* header styles */\n"
        "header { /* inline comment */ height: 60px; }\n");
    CHECK(rules.size() == 1);
    const CSSRule* h = findRule(rules, "header");
    CHECK(h != nullptr);
    CHECK(h->declarations.at("height") == "60px");
}

static void testParseAtRules() {
    CSSParser parser;
    auto rules = parser.parse(
        "@media (max-width: 600px) { body { font-size: 14px; } }\n"
        "@font-face { font-family: \"MyFont\"; src: url(\"my.woff\"); }\n"
        "p { color: green; }\n");

    // Locate the @media at-rule.
    const CSSRule* media = nullptr;
    const CSSRule* fontFace = nullptr;
    const CSSRule* p = nullptr;
    for (const auto& r : rules) {
        if (r.type == RuleType::At && r.selector.find("media") != std::string::npos)
            media = &r;
        if (r.type == RuleType::At && r.selector.find("font-face") != std::string::npos)
            fontFace = &r;
        if (r.type == RuleType::Style && r.selector == "p") p = &r;
    }

    CHECK(media != nullptr);
    CHECK(media->selector.find("max-width") != std::string::npos);
    // @media wraps a nested body rule.
    CHECK(media->nested.size() == 1);
    CHECK(media->nested[0].selector == "body");
    CHECK(media->nested[0].declarations.at("font-size") == "14px");

    CHECK(fontFace != nullptr);
    CHECK(fontFace->declarations.count("font-family") == 1);
    CHECK(fontFace->declarations.at("src") == "url(my.woff)");

    CHECK(p != nullptr);
    CHECK(p->declarations.at("color") == "green");
}

// ── inline style tests ───────────────────────────────────────────────────────

static void testInlineStyle() {
    auto decls = CSSParser::parseInlineStyle("color: red; font-size: 24px; text-align: center");
    CHECK(decls.size() == 3);
    CHECK(decls.at("color") == "red");
    CHECK(decls.at("font-size") == "24px");
    CHECK(decls.at("text-align") == "center");
}

static void testInlineStyleFunctional() {
    auto decls = CSSParser::parseInlineStyle(
        "background: url('bg.png'); border: 1px solid rgb(0, 0, 0);");
    CHECK(decls.at("background") == "url(bg.png)");
    CHECK(decls.at("border") == "1px solid rgb(0, 0, 0)");
}

static void testInlineStyleEmpty() {
    auto decls = CSSParser::parseInlineStyle("");
    CHECK(decls.empty());

    auto decls2 = CSSParser::parseInlineStyle("   ;;  ");
    CHECK(decls2.empty());
}

// ── stylesheet integration ───────────────────────────────────────────────────

static void testFullStylesheet() {
    CSSParser parser;
    auto rules = parser.parse(
        "/* Nitrogen demo stylesheet */\n"
        "* { box-sizing: border-box; }\n"
        "body {\n"
        "  margin: 0;\n"
        "  font-family: Arial, sans-serif;\n"
        "  color: #333333;\n"
        "}\n"
        ".container > .row {\n"
        "  display: flex;\n"
        "  padding: 10px 20px;\n"
        "}\n"
        "a:hover { text-decoration: underline; }\n");

    // Universal selector.
    const CSSRule* uni = findRule(rules, "*");
    CHECK(uni != nullptr);
    CHECK(uni->declarations.at("box-sizing") == "border-box");

    const CSSRule* body = findRule(rules, "body");
    CHECK(body != nullptr);
    CHECK(body->declarations.size() == 3);
    CHECK(body->declarations.at("font-family") == "Arial, sans-serif");
    CHECK(body->declarations.at("color") == "#333333");

    const CSSRule* row = findRule(rules, ".container > .row");
    CHECK(row != nullptr);
    CHECK(row->declarations.at("display") == "flex");
    CHECK(row->declarations.at("padding") == "10px 20px");

    const CSSRule* hover = findRule(rules, "a:hover");
    CHECK(hover != nullptr);
    CHECK(hover->declarations.at("text-decoration") == "underline");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    testTokenizerBasics();
    testTokenizerNumbersAndStrings();
    testTokenizerFunctionsAndUrls();

    testParseSimpleRules();
    testParseSelectorKinds();
    testParseCompoundSelector();
    testParseComments();
    testParseAtRules();

    testInlineStyle();
    testInlineStyleFunctional();
    testInlineStyleEmpty();

    testFullStylesheet();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
