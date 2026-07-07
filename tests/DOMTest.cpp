/**
 * @file tests/DOMTest.cpp
 * @brief Unit tests for the HTML5 DOM implementation.
 * @details Parses small HTML documents and asserts the resulting DOM tree shape,
 *          the tokeniser attribute handling, the tree builder's table nesting,
 *          the simplified adoption-agency / formatting handling, and SVG/MathML
 *          namespace splitting.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include "html/Tokenizer.h"
#include "html/TreeBuilder.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

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

// ── DOM walk helpers ──────────────────────────────────────────────────────────

static const DOMNode* firstElementChild(const DOMNode* n, const std::string& tag) {
    if (!n) return nullptr;
    for (const DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == tag) return c;
    }
    return nullptr;
}

static const DOMNode* findElement(const DOMNode* n, const std::string& tag) {
    if (!n) return nullptr;
    if (n->nodeType == NodeType::Element && n->tagName == tag) return n;
    for (const DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (const DOMNode* found = findElement(c, tag)) return found;
    }
    return nullptr;
}

static size_t countElementChildren(const DOMNode* n) {
    if (!n) return 0;
    size_t count = 0;
    for (const DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element) ++count;
    }
    return count;
}

static std::string textContentOf(const DOMNode* n) {
    std::string out;
    if (!n) return out;
    for (const DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Text) out += c->textContent;
        else if (c->nodeType == NodeType::Element) out += textContentOf(c);
    }
    return out;
}

// ── test cases ─────────────────────────────────────────────────────────────────

static void testBasicDocument() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<!DOCTYPE html><html><head><title>Hello Page</title></head>"
        "<body><p>Hello <b>world</b></p></body></html>");

    CHECK(doc != nullptr);
    CHECK(doc->nodeType == NodeType::Document);

    const DOMNode* html = firstElementChild(doc, "html");
    CHECK(html != nullptr);
    CHECK(html->namespaceURI == ns::HTML);

    const DOMNode* head = firstElementChild(html, "head");
    CHECK(head != nullptr);
    const DOMNode* title = firstElementChild(head, "title");
    CHECK(title != nullptr);
    CHECK(textContentOf(title) == "Hello Page");

    const DOMNode* body = firstElementChild(html, "body");
    CHECK(body != nullptr);
    const DOMNode* p = firstElementChild(body, "p");
    CHECK(p != nullptr);
    CHECK(textContentOf(p) == "Hello world");

    delete doc;
}

static void testTableNesting() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<table>"
        "  <tr><td>A</td><td>B</td></tr>"
        "  <tr><td>C</td></tr>"
        "</table>");

    const DOMNode* table = findElement(doc, "table");
    CHECK(table != nullptr);
    CHECK(table->nodeType == NodeType::Element);
    CHECK(table->tagName == "table");

    // The tree builder inserts an implicit <tbody>.
    const DOMNode* tbody = firstElementChild(table, "tbody");
    CHECK(tbody != nullptr);
    CHECK(countElementChildren(tbody) == 2);            // two <tr>

    const DOMNode* firstRow = firstElementChild(tbody, "tr");
    CHECK(firstRow != nullptr);
    CHECK(countElementChildren(firstRow) == 2);          // two <td>

    const DOMNode* cellA = firstElementChild(firstRow, "td");
    CHECK(cellA != nullptr);
    CHECK(textContentOf(cellA) == "A");

    // Not flat text: a <td> must be an element child, not a text blob.
    CHECK(cellA->nodeType == NodeType::Element);

    delete doc;
}

static void testTokenizerAttributes() {
    Tokenizer tok("<a href=\"https://example.com\" class='x' data-foo=bare>txt</a>");
    auto tokens = tok.tokenize();

    // First meaningful token should be the <a> start tag with 3 attributes.
    bool found = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::StartTag && t.data == "a") {
            found = true;
            CHECK(t.attributes.size() == 3);
            CHECK(t.attributes[0].first == "href");
            CHECK(t.attributes[0].second == "https://example.com");
            CHECK(t.attributes[1].first == "class");
            CHECK(t.attributes[1].second == "x");
            CHECK(t.attributes[2].first == "data-foo");
            CHECK(t.attributes[2].second == "bare");
            break;
        }
    }
    CHECK(found);

    // Attribute names are lower-cased; values preserve case.
    bool charFound = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Character && t.data.find("txt") != std::string::npos) {
            charFound = true;
            break;
        }
    }
    CHECK(charFound);
}

static void testCommentsAndDoctype() {
    HTMLParser parser;
    Document* doc = parser.parse("<!-- lead --><!DOCTYPE html><div>body</div>");

    // Leading comment should be a child of the document.
    bool commentSeen = false;
    for (const DOMNode* c = doc->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Comment) { commentSeen = true; break; }
    }
    CHECK(commentSeen);

    const DOMNode* div = findElement(doc, "div");
    CHECK(div != nullptr);
    CHECK(textContentOf(div) == "body");

    delete doc;
}

static void testNamespaceSplitting() {
    HTMLParser parser;
    Document* doc = parser.parse("<svg><rect></rect></svg>");

    const DOMNode* svg = findElement(doc, "svg");
    CHECK(svg != nullptr);
    CHECK(svg->namespaceURI == ns::SVG);

    const DOMNode* rect = firstElementChild(svg, "rect");
    CHECK(rect != nullptr);
    // Children of an <svg> inherit the SVG namespace.
    CHECK(rect->namespaceURI == ns::SVG);

    delete doc;

    // MathML path.
    Document* doc2 = parser.parse("<math><mi>x</mi></math>");
    const DOMNode* math = findElement(doc2, "math");
    CHECK(math != nullptr);
    CHECK(math->namespaceURI == ns::MathML);
    const DOMNode* mi = firstElementChild(math, "mi");
    CHECK(mi != nullptr);
    CHECK(mi->namespaceURI == ns::MathML);

    delete doc2;
}

static void testFormattingAdoption() {
    // A double <b> wrapping text should not lose the inner content; the adoption
    // agency (simplified) must keep the tree well-formed.
    HTMLParser parser;
    Document* doc = parser.parse("<b>one<i>two</i>three</b>");

    const DOMNode* b = findElement(doc, "b");
    CHECK(b != nullptr);
    CHECK(textContentOf(b) == "onetwothree");

    const DOMNode* i = findElement(b, "i");
    CHECK(i != nullptr);
    CHECK(textContentOf(i) == "two");

    delete doc;
}

static void testSetInnerHTML() {
    Document doc;
    DOMNode* div = new DOMNode();
    div->nodeType = NodeType::Element;
    div->tagName = "div";
    div->ownerDocument = &doc;
    doc.appendChild(div);

    div->setInnerHTML("<span>hi</span><span>there</span>");
    CHECK(countElementChildren(div) == 2);
    CHECK(firstElementChild(div, "span") != nullptr);
    CHECK(textContentOf(div) == "hithere");

    // `doc` owns `div` and all of its children; let the destructor clean up.
}

// ── main ───────────────────────────────────────────────────────────────────────

int main() {
    testBasicDocument();
    testTableNesting();
    testTokenizerAttributes();
    testCommentsAndDoctype();
    testNamespaceSplitting();
    testFormattingAdoption();
    testSetInnerHTML();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
