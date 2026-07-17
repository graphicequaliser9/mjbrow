/**
 * @file tests/CascadeTest.cpp
 * @brief Unit tests for the CSS cascade engine (Bead B).
 * @details Builds a small DOM tree by hand, applies a set of authored CSS
 *          rules (produced by the Bead A parser) plus inline styles, and
 *          asserts the final computed property map for target elements.
 *          Covers type / class / id selectors, descendant and child
 *          combinators, specificity ordering, source-order tie-breaking and
 *          :visited / :link / :hover pseudo-class no-ops.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/Cascade.h"
#include "css/CSSParser.h"
#include "html/DOMNode.h"

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace css;
using namespace html;

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

// ── DOM tree helpers ─────────────────────────────────────────────────────────

static DOMNode* makeElement(const std::string& tag, const std::string& id = "",
                            const std::string& cls = "") {
    DOMNode* n = new DOMNode();
    n->nodeType = NodeType::Element;
    n->tagName = tag;
    if (!id.empty()) n->setAttribute("id", id);
    if (!cls.empty()) n->setAttribute("class", cls);
    return n;
}

// ── specificity tests ────────────────────────────────────────────────────────

static void testSpecificityRanking() {
    Specificity a = Cascade::specificityOf("div");
    Specificity b = Cascade::specificityOf(".btn");
    Specificity c = Cascade::specificityOf("#main");
    Specificity d = Cascade::specificityOf("div.box#inner");
    Specificity e = Cascade::specificityOf("a:hover");

    CHECK(a.typeCount == 1 && a.idCount == 0 && a.classCount == 0);
    CHECK(b.classCount == 1 && b.idCount == 0 && b.typeCount == 0);
    CHECK(c.idCount == 1);
    // compound: type(1) + class(1) + id(1)
    CHECK(d.typeCount == 1 && d.classCount == 1 && d.idCount == 1);
    // :hover counts as a class component
    CHECK(e.classCount == 1 && e.typeCount == 1);

    // ordering: inline(0) id > class > type
    CHECK(!(c < b) && b < c);   // id wins over class
    CHECK(!(b < a) && a < b);   // class wins over type
    CHECK(a < d);               // compound beats bare type
}

static void testSpecificitySourceOrderTiebreak() {
    Specificity a = Cascade::specificityOf(".a");
    Specificity b = Cascade::specificityOf(".b");
    CHECK(!(a < b) && !(b < a));  // equal specificity
    a.sourceOrder = 0;
    b.sourceOrder = 1;
    CHECK(a < b);                  // source order breaks the tie
}

// ── matching tests ───────────────────────────────────────────────────────────

static void testSelectorMatching() {
    // Build: html > body > div#main.box > p.txt > span
    DOMNode* body = makeElement("body");
    DOMNode* div = makeElement("div", "main", "box");
    DOMNode* p = makeElement("p", "", "txt");
    DOMNode* span = makeElement("span");
    body->appendChild(div);
    div->appendChild(p);
    p->appendChild(span);

    CHECK(Cascade::matches(div, "div"));
    CHECK(Cascade::matches(div, ".box"));
    CHECK(Cascade::matches(div, "#main"));
    CHECK(Cascade::matches(div, "div.box#main"));
    CHECK(Cascade::matches(div, "body div"));                 // descendant
    CHECK(Cascade::matches(div, "body > div"));               // child
    CHECK(Cascade::matches(span, "body p span"));             // deep descendant
    CHECK(Cascade::matches(span, "div > p > span"));          // child chain
    CHECK(Cascade::matches(p, "div p.txt"));                  // compound + descendant

    // negative cases
    CHECK(!Cascade::matches(div, "p"));
    CHECK(!Cascade::matches(div, ".nope"));
    CHECK(!Cascade::matches(div, "#wrong"));
    CHECK(!Cascade::matches(div, "body > p"));                // div is not a direct child of body's p
    CHECK(!Cascade::matches(span, "div > span"));             // span is grandchild, not child

    delete body;  // recursive destructor cleans the subtree
}

static void testPseudoClassNoOp() {
    DOMNode* a = makeElement("a", "lnk");
    CHECK(Cascade::matches(a, "a:visited"));
    CHECK(Cascade::matches(a, "a:link"));
    CHECK(Cascade::matches(a, "a:hover"));
    CHECK(Cascade::matches(a, "a:link:hover"));
    delete a;
}

// ── cascade / computed map tests ─────────────────────────────────────────────

static void testComputeBasicCascade() {
    // Tree: div#main.box > p
    DOMNode* div = makeElement("div", "main", "box");
    DOMNode* p = makeElement("p");
    div->appendChild(p);

    CSSParser parser;
    auto rules = parser.parse(
        "div { color: black; padding: 4px; }\n"
        ".box { color: green; }\n"
        "#main { color: red; font-size: 20px; }\n"
        "p { color: blue; font-size: 12px; }\n");

    // The div matches div / .box / #main.  Highest specificity is #main (id),
    // so color: red wins; font-size comes only from #main.
    auto divStyle = Cascade::computeStyle(div, rules);
    CHECK(divStyle.at("color") == "red");
    CHECK(divStyle.at("font-size") == "20px");
    CHECK(divStyle.at("padding") == "4px");

    // The p matches only p.
    auto pStyle = Cascade::computeStyle(p, rules);
    CHECK(pStyle.at("color") == "blue");
    CHECK(pStyle.at("font-size") == "12px");

    delete div;
}

static void testSourceOrderTiebreak() {
    DOMNode* a = makeElement("a", "", "link");

    CSSParser parser;
    auto rules = parser.parse(
        ".link { color: red; }\n"
        "a.link { color: blue; }\n"   // equal specificity to next line -> later wins
        "a.link { color: green; }\n");

    auto s = Cascade::computeStyle(a, rules);
    // a.link (class+type) beats .link (class): both green/blue apply, blue/green
    // are equal specificity, source order -> green (last).
    CHECK(s.at("color") == "green");

    delete a;
}

static void testInlineWins() {
    DOMNode* div = makeElement("div", "main");
    div->setAttribute("style", "color: purple; font-size: 30px;");

    CSSParser parser;
    auto rules = parser.parse("#main { color: red; font-size: 20px; }");

    auto s = Cascade::computeStyle(div, rules);
    CHECK(s.at("color") == "purple");     // inline overrides id rule
    CHECK(s.at("font-size") == "30px");

    delete div;
}

static void testPseudoRuleMatchesAll() {
    // :hover rules should apply to the element (no-op pseudo).
    DOMNode* a = makeElement("a", "x");
    CSSParser parser;
    auto rules = parser.parse("a:hover { color: orange; }");
    auto s = Cascade::computeStyle(a, rules);
    CHECK(s.at("color") == "orange");
    delete a;
}

static void testDescendantVsChildCascade() {
    // body > div.container > p, and also div.wrap p (descendant)
    DOMNode* body = makeElement("body");
    DOMNode* container = makeElement("div", "", "container");
    DOMNode* p = makeElement("p", "", "lead");
    DOMNode* deeper = makeElement("span");
    body->appendChild(container);
    container->appendChild(p);
    p->appendChild(deeper);

    CSSParser parser;
    auto rules = parser.parse(
        "div p { color: navy; }\n"            // descendant -> matches p
        "div > p { font-weight: bold; }\n"    // child -> matches p only
        "p span { color: teal; }\n"           // descendant -> matches deeper
        "p > span { font-weight: italic; }\n" // child -> matches deeper only
    );

    auto ps = Cascade::computeStyle(p, rules);
    CHECK(ps.at("color") == "navy");           // div p descendant
    CHECK(ps.at("font-weight") == "bold");     // div > p child

    auto ss = Cascade::computeStyle(deeper, rules);
    CHECK(ss.at("color") == "teal");           // p span descendant
    CHECK(ss.at("font-weight") == "italic");   // p > span child

    delete body;
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    testSpecificityRanking();
    testSpecificitySourceOrderTiebreak();
    testSelectorMatching();
    testPseudoClassNoOp();

    testComputeBasicCascade();
    testSourceOrderTiebreak();
    testInlineWins();
    testPseudoRuleMatchesAll();
    testDescendantVsChildCascade();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
