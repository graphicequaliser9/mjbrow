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
#include <fstream>
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

    std::string html = div->getInnerHTML();
    CHECK(html.find("<span>hi</span>") != std::string::npos);
    CHECK(html.find("<span>there</span>") != std::string::npos);

    // `doc` owns `div` and all of its children; let the destructor clean up.
}

// ── manual DOM tree construction + traversal (Bead 1) ──────────────────────────

static void testManualTreeTraversal() {
    // Build manually:
    //   doc
    //    └─ html
    //         ├─ head
    //         └─ body
    //              ├─ p  ("Hello ")
    //              └─ div
    //                   └─ span ("world")
    Document doc;

    DOMNode* html = new DOMNode();
    html->nodeType = NodeType::Element;
    html->tagName = "html";
    doc.appendChild(html);

    DOMNode* head = new DOMNode();
    head->nodeType = NodeType::Element;
    head->tagName = "head";
    html->appendChild(head);

    DOMNode* body = new DOMNode();
    body->nodeType = NodeType::Element;
    body->tagName = "body";
    html->appendChild(body);

    DOMNode* p = new DOMNode();
    p->nodeType = NodeType::Element;
    p->tagName = "p";
    p->setAttribute("class", "lead");
    body->appendChild(p);

    DOMNode* pText = new DOMNode();
    pText->nodeType = NodeType::Text;
    pText->textContent = "Hello ";
    p->appendChild(pText);

    DOMNode* div = new DOMNode();
    div->nodeType = NodeType::Element;
    div->tagName = "div";
    body->appendChild(div);

    DOMNode* span = new DOMNode();
    span->nodeType = NodeType::Element;
    span->tagName = "span";
    div->appendChild(span);

    DOMNode* spanText = new DOMNode();
    spanText->nodeType = NodeType::Text;
    spanText->textContent = "world";
    span->appendChild(spanText);

    // --- traversal checks ---
    CHECK(doc.firstChild == html);
    CHECK(html->parent == &doc);
    CHECK(html->firstChild == head);
    CHECK(html->lastChild == body);
    CHECK(head->nextSibling == body);
    CHECK(body->prevSibling == head);
    CHECK(body->firstChild == p);
    CHECK(body->lastChild == div);
    CHECK(p->nextSibling == div);
    CHECK(div->prevSibling == p);
    CHECK(p->firstChild == pText);
    CHECK(div->firstChild == span);
    CHECK(span->firstChild == spanText);

    // --- ownerDocument propagation (Document owns itself + subtree) ---
    CHECK(html->ownerDocument == &doc);
    CHECK(body->ownerDocument == &doc);
    CHECK(p->ownerDocument == &doc);
    CHECK(spanText->ownerDocument == &doc);

    // --- attribute helpers ---
    CHECK(p->hasAttribute("class"));
    CHECK(*p->getAttribute("class") == "lead");
    CHECK(!p->hasAttribute("id"));
    p->setAttribute("id", "intro");
    CHECK(*p->getAttribute("id") == "intro");
    CHECK(p->hasAttributes());
    p->removeAttribute("class");
    CHECK(!p->hasAttribute("class"));

    // --- textContent extraction via recursive walk ---
    CHECK(textContentOf(&doc) == "Hello world");

    // `doc` owns the whole subtree; its destructor frees everything.
}

static void testInsertBeforeReplaceRemove() {
    Document doc;
    DOMNode* root = new DOMNode();
    root->nodeType = NodeType::Element;
    root->tagName = "root";
    doc.appendChild(root);

    DOMNode* a = new DOMNode(); a->nodeType = NodeType::Element; a->tagName = "a";
    DOMNode* b = new DOMNode(); b->nodeType = NodeType::Element; b->tagName = "b";
    DOMNode* c = new DOMNode(); c->nodeType = NodeType::Element; c->tagName = "c";
    root->appendChild(a);
    root->appendChild(b);
    root->appendChild(c);

    // Insert x before b: order becomes a, x, b, c.
    DOMNode* x = new DOMNode(); x->nodeType = NodeType::Element; x->tagName = "x";
    root->insertBefore(x, b);
    CHECK(root->firstChild == a);
    CHECK(a->nextSibling == x);
    CHECK(x->nextSibling == b);
    CHECK(b->prevSibling == x);
    CHECK(x->ownerDocument == &doc);

    // insertBefore with null child appends.
    DOMNode* y = new DOMNode(); y->nodeType = NodeType::Element; y->tagName = "y";
    root->insertBefore(y, nullptr);
    CHECK(root->lastChild == y);

    // Replace b with z: order a, x, z, c, y. The old node `b` is freed by
    // replaceChild, so we only assert on the surviving (valid) nodes.
    DOMNode* z = new DOMNode(); z->nodeType = NodeType::Element; z->tagName = "z";
    root->replaceChild(z, b);
    CHECK(x->nextSibling == z);
    CHECK(z->nextSibling == c);
    CHECK(z->prevSibling == x);
    CHECK(z->ownerDocument == &doc);

    // removeChild y.
    root->removeChild(y);
    CHECK(root->lastChild == c);
    CHECK(c->nextSibling == nullptr);

    // Move semantics: re-append a (already a child) keeps a single copy.
    size_t count = 0;
    for (DOMNode* n = root->firstChild; n; n = n->nextSibling) ++count;
    root->appendChild(a);   // move a to the end
    size_t count2 = 0;
    for (DOMNode* n = root->firstChild; n; n = n->nextSibling) ++count2;
    CHECK(count == count2);  // same number of children (no duplication)
    CHECK(root->lastChild == a);

    // delete doc frees the entire tree.
}

static void testCloneNode() {
    Document doc;
    DOMNode* div = new DOMNode();
    div->nodeType = NodeType::Element;
    div->tagName = "div";
    div->setAttribute("id", "box");
    doc.appendChild(div);

    DOMNode* p = new DOMNode();
    p->nodeType = NodeType::Element; p->tagName = "p";
    div->appendChild(p);

    DOMNode* t = new DOMNode();
    t->nodeType = NodeType::Text; t->textContent = "hi";
    p->appendChild(t);

    // Shallow clone: only the div, no children.
    DOMNode* shallow = div->cloneNode(false);
    CHECK(shallow->nodeType == NodeType::Element);
    CHECK(shallow->tagName == "div");
    CHECK(*shallow->getAttribute("id") == "box");
    CHECK(shallow->firstChild == nullptr);
    CHECK(shallow->ownerDocument == nullptr);  // clone is detached
    delete shallow;

    // Deep clone: full subtree, independent copy.
    DOMNode* deep = div->cloneNode(true);
    CHECK(deep->firstChild != nullptr);
    DOMNode* cp = const_cast<DOMNode*>(firstElementChild(deep, "p"));
    CHECK(cp != nullptr);
    CHECK(textContentOf(cp) == "hi");
    // Mutating the clone must not affect the original.
    cp->setAttribute("class", "mutated");
    CHECK(!p->hasAttribute("class"));
    delete deep;

    // Document deep clone carries the doctype and a fresh owning document.
    doc.doctype = "html";
    Document* docClone = static_cast<Document*>(doc.cloneNode(true));
    CHECK(docClone->doctype == "html");
    CHECK(docClone->firstChild != nullptr);
    CHECK(docClone->firstChild->ownerDocument == docClone);
    delete docClone;
}

static void testDoctypeCapture() {
    HTMLParser parser;
    Document* doc = parser.parse("<!DOCTYPE html><html><body>x</body></html>");
    CHECK(doc->doctype == "html");
    delete doc;

    // No DOCTYPE -> empty doctype, parse still succeeds.
    Document* doc2 = parser.parse("<html><body>y</body></html>");
    CHECK(doc2->doctype.empty());
    delete doc2;
}

static void testAttributeNodeType() {
    // The Attribute node type is usable as a standalone node.
    DOMNode* attr = new DOMNode();
    attr->nodeType = NodeType::Attribute;
    attr->tagName = "href";
    attr->textContent = "https://example.com";
    CHECK(attr->nodeType == NodeType::Attribute);
    delete attr;
}

// ── Bead 2 tokenizer tests ─────────────────────────────────────────────────────

static void testBead2BasicTokenSequence() {
    Tokenizer tok("<div class='test' id=123>Hello <!-- comment --></div>");
    auto tokens = tok.tokenize();

    CHECK(tokens.size() >= 5);

    int idx = 0;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "div");
    CHECK(tokens[idx].attributes.size() == 2);
    CHECK(tokens[idx].attributes[0].first == "class");
    CHECK(tokens[idx].attributes[0].second == "test");
    CHECK(tokens[idx].attributes[1].first == "id");
    CHECK(tokens[idx].attributes[1].second == "123");
    CHECK(!tokens[idx].selfClosing);

    ++idx;
    CHECK(tokens[idx].type == TokenType::Character);
    CHECK(tokens[idx].data == "Hello ");

    ++idx;
    CHECK(tokens[idx].type == TokenType::Comment);
    CHECK(tokens[idx].data.find("comment") != std::string::npos);

    ++idx;
    CHECK(tokens[idx].type == TokenType::EndTag);
    CHECK(tokens[idx].data == "div");

    ++idx;
    CHECK(tokens[idx].type == TokenType::EOF_TOKEN);
}

static void testSelfClosingTags() {
    Tokenizer tok("<img src='x.png' alt=desc/><br/><input type=text disabled/>");
    auto tokens = tok.tokenize();

    int idx = 0;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "img");
    CHECK(tokens[idx].selfClosing);
    CHECK(tokens[idx].attributes.size() == 2);

    ++idx;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "br");
    CHECK(tokens[idx].selfClosing);

    ++idx;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "input");
    CHECK(tokens[idx].selfClosing);
    CHECK(tokens[idx].attributes.size() == 2);

    CHECK(tokens.back().type == TokenType::EOF_TOKEN);
}

static void testScriptStyleRawText() {
    Tokenizer tok("<script>var x = a < b;</script><style>body { color: red; }</style>");
    auto tokens = tok.tokenize();

    int idx = 0;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "script");

    ++idx;
    CHECK(tokens[idx].type == TokenType::Character);
    CHECK(tokens[idx].data == "var x = a < b;");

    ++idx;
    CHECK(tokens[idx].type == TokenType::EndTag);
    CHECK(tokens[idx].data == "script");

    ++idx;
    CHECK(tokens[idx].type == TokenType::StartTag);
    CHECK(tokens[idx].data == "style");

    ++idx;
    CHECK(tokens[idx].type == TokenType::Character);
    CHECK(tokens[idx].data == "body { color: red; }");

    ++idx;
    CHECK(tokens[idx].type == TokenType::EndTag);
    CHECK(tokens[idx].data == "style");

    CHECK(tokens.back().type == TokenType::EOF_TOKEN);
}

static void testMalformedHtml() {
    // Missing end tags, stray closing tags, and no attributes should all work.
    Tokenizer tok("<p>unclosed<span>data</p></p><!-- un closed comment");
    auto tokens = tok.tokenize();

    int startTags = 0;
    int endTags = 0;
    int comments = 0;
    int chars = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::StartTag) ++startTags;
        else if (t.type == TokenType::EndTag) ++endTags;
        else if (t.type == TokenType::Comment) ++comments;
        else if (t.type == TokenType::Character) ++chars;
    }
    CHECK(startTags >= 2);
    CHECK(endTags >= 1);
    CHECK(chars > 0);
    CHECK(comments >= 1);
}

static void testDoctypeToken() {
    Tokenizer tok("<!doctype html><html><body>x</body></html>");
    auto tokens = tok.tokenize();

    CHECK(tokens[0].type == TokenType::DOCTYPE);
    CHECK(tokens[0].data == "html");

    CHECK(tokens.back().type == TokenType::EOF_TOKEN);
}

static void testFosterParenting() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<table><tr><td>A</td></tr></table><p>After table</p>");

    const DOMNode* table = findElement(doc, "table");
    CHECK(table != nullptr);

    const DOMNode* body = table->parent;
    CHECK(body != nullptr);
    CHECK(body->nodeType == NodeType::Element);
    CHECK(body->tagName == "body");

    bool foundAfterTable = false;
    for (const DOMNode* c = body->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "p") {
            foundAfterTable = true;
            CHECK(textContentOf(c) == "After table");
        }
    }
    CHECK(foundAfterTable);

    delete doc;
}

static void testFosterParentTextInTable() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<table>text<tr><td>A</td></tr></table>");

    const DOMNode* table = findElement(doc, "table");
    CHECK(table != nullptr);

    const DOMNode* body = table->parent;
    CHECK(body != nullptr);

    // "text" should be foster-parented as a text node before <table> in its parent.
    bool foundTextSibling = false;
    for (const DOMNode* c = body->firstChild; c; c = c->nextSibling) {
        if (c == table) break;
        if (c->nodeType == NodeType::Text && c->textContent.find("text") != std::string::npos) {
            foundTextSibling = true;
        }
    }
    CHECK(foundTextSibling);

    delete doc;
}

static void testImplicitPClose() {
    // <p> without closing tag must be closed when a <div> starts.
    HTMLParser parser;
    Document* doc = parser.parse(
        "<html><body><p>Hello<div>world</div></body></html>");

    const DOMNode* body = findElement(doc, "body");
    CHECK(body != nullptr);

    const DOMNode* p = firstElementChild(body, "p");
    CHECK(p != nullptr);
    CHECK(textContentOf(p) == "Hello");

    const DOMNode* div = firstElementChild(body, "div");
    CHECK(div != nullptr);
    // Verify <p> and <div> are siblings, not nested.
    CHECK(p->nextSibling == div);

    delete doc;

    // Two consecutive <p> blocks: the first must be auto-closed.
    Document* doc2 = parser.parse(
        "<p>First<p>Second</p>");
    const DOMNode* body2 = findElement(doc2, "body");
    CHECK(body2 != nullptr);

    std::vector<const DOMNode*> ps;
    for (const DOMNode* c = body2->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "p") ps.push_back(c);
    }
    CHECK(ps.size() == 2);
    CHECK(textContentOf(ps[0]) == "First");
    CHECK(textContentOf(ps[1]) == "Second");
    CHECK(ps[0]->nextSibling == ps[1]);

    delete doc2;
}

static void testComplexHtml() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<html><head><title>Complex</title></head><body>"
        "<h1>Title</h1>"
        "<p>Intro</p>"
        "<table>"
        "  <tr><th>A</th><th>B</th></tr>"
        "  <tr><td>1</td><td>2</td></tr>"
        "  <tr>"
        "    <td>"
        "      <table><tr><td>Nested</td></tr></table>"
        "    </td>"
        "  </tr>"
        "</table>"
        "<ul>"
        "  <li>Item 1</li>"
        "  <li>Item 2</li>"
        "</ul>"
        "<form>"
        "  <input type='text' value='x'/>"
        "  <br/>"
        "  <textarea>txt</textarea>"
        "</form>"
        "<hr/>"
        "<p>Outro</p>"
        "</body></html>");

    CHECK(doc != nullptr);

    const DOMNode* body = findElement(doc, "body");
    CHECK(body != nullptr);

    const DOMNode* h1 = firstElementChild(body, "h1");
    CHECK(h1 != nullptr);
    CHECK(textContentOf(h1) == "Title");

    const DOMNode* p1 = firstElementChild(body, "p");
    CHECK(p1 != nullptr);
    CHECK(textContentOf(p1) == "Intro");

    const DOMNode* outerTable = findElement(doc, "table");
    CHECK(outerTable != nullptr);
    const DOMNode* outerTbody = firstElementChild(outerTable, "tbody");
    CHECK(outerTbody != nullptr);
    CHECK(countElementChildren(outerTbody) == 3); // three rows

    const DOMNode* innerTable = findElement(outerTbody, "table");
    CHECK(innerTable != nullptr);
    const DOMNode* innerTd = findElement(innerTable, "td");
    CHECK(innerTd != nullptr);
    CHECK(textContentOf(innerTd) == "Nested");

    const DOMNode* ul = findElement(doc, "ul");
    CHECK(ul != nullptr);
    CHECK(countElementChildren(ul) == 2);

    const DOMNode* form = findElement(doc, "form");
    CHECK(form != nullptr);
    CHECK(findElement(form, "input") != nullptr);

    const DOMNode* hr = findElement(doc, "hr");
    CHECK(hr != nullptr);

    const DOMNode* p2 = nullptr;
    for (const DOMNode* c = body->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "p") p2 = c;
    }
    CHECK(p2 != nullptr);
    CHECK(textContentOf(p2) == "Outro");

    delete doc;
}

static void testVoidElementsInBody() {
    HTMLParser parser;
    Document* doc = parser.parse(
        "<html><body><br/><img src='x.png' alt='y'/><hr/><input type='text'/></body></html>");

    const DOMNode* body = findElement(doc, "body");
    CHECK(body != nullptr);

    CHECK(firstElementChild(body, "br") != nullptr);
    CHECK(firstElementChild(body, "img") != nullptr);
    CHECK(firstElementChild(body, "hr") != nullptr);
    CHECK(firstElementChild(body, "input") != nullptr);

    const DOMNode* imgEl = firstElementChild(body, "img");
    CHECK(imgEl != nullptr);
    CHECK(imgEl->attributes.count("src") == 1);
    CHECK(*imgEl->getAttribute("src") == "x.png");

    delete doc;
}

static void testBrowtest3LargeTable() {
    std::ifstream in(TEST_DATA_DIR "/browtest3.htm");
    CHECK(in.is_open());
    std::string html((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    in.close();

    HTMLParser parser;
    Document* doc = parser.parse(html);
    CHECK(doc != nullptr);

    const DOMNode* table = findElement(doc, "table");
    CHECK(table != nullptr);

    const DOMNode* tbody = firstElementChild(table, "tbody");
    CHECK(tbody != nullptr);

    size_t trCount = 0;
    for (const DOMNode* c = tbody->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "tr") ++trCount;
    }
    CHECK(trCount >= 3500);

    const DOMNode* firstRow = firstElementChild(tbody, "tr");
    CHECK(firstRow != nullptr);
    CHECK(countElementChildren(firstRow) == 4);

    const DOMNode* firstTd = firstElementChild(firstRow, "td");
    CHECK(firstTd != nullptr);
    CHECK(firstTd->nodeType == NodeType::Element);

    delete doc;
}


// ── Bead 4: DOM integration + query API ───────────────────────────────────────

// Helper: extract text only from the <body> element (legacy "body-only" view).
static std::string bodyOnlyText(const DOMNode* doc) {
    const DOMNode* body = nullptr;
    for (const DOMNode* c = doc->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Element && c->tagName == "html") {
            for (const DOMNode* h = c->firstChild; h; h = h->nextSibling) {
                if (h->nodeType == NodeType::Element && h->tagName == "body") { body = h; break; }
            }
            break;
        }
    }
    if (!body) return "";
    std::string out;
    for (const DOMNode* c = body->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == NodeType::Text) out += c->textContent;
        else if (c->nodeType == NodeType::Element) out += textContentOf(c);
    }
    return out;
}

static void testBead4NestedPageDOM() {
    // A page with nested divs, spans, and tables plus head content.
    HTMLParser parser;
    Document* doc = parser.parse(
        "<!DOCTYPE html>"
        "<html>"
        "  <head><title>Page Heading</title></head>"
        "  <body>"
        "    <div id='outer' class='wrap'>"
        "      <div id='inner' class='box highlight'>"
        "        <span class='label'>Hello</span>"
        "        <span class='value'>World</span>"
        "      </div>"
        "      <table id='grid'>"
        "        <tr><td>A1</td><td>A2</td></tr>"
        "        <tr><td>B1</td><td>B2</td></tr>"
        "      </table>"
        "    </div>"
        "  </body>"
        "</html>");

    CHECK(doc != nullptr);

    // --- DOM tree built correctly: nested divs ---
    const DOMNode* outer = doc->getElementById("outer");
    CHECK(outer != nullptr);
    CHECK(outer->tagName == "div");
    CHECK(outer->hasAttribute("class"));
    CHECK(*outer->getAttribute("class") == "wrap");

    const DOMNode* inner = doc->getElementById("inner");
    CHECK(inner != nullptr);
    // #inner is nested inside #outer (descendant), not a sibling.
    bool isDescendant = false;
    for (const DOMNode* c = outer->firstChild; c; c = c->nextSibling) {
        if (c == inner) { isDescendant = true; break; }
    }
    CHECK(isDescendant);

    // --- DOM tree built correctly: table nesting ---
    const DOMNode* grid = doc->getElementById("grid");
    CHECK(grid != nullptr);
    const DOMNode* tbody = firstElementChild(grid, "tbody");
    CHECK(tbody != nullptr);
    CHECK(countElementChildren(tbody) == 2);   // two <tr>

    // --- query API: getElementsByTagName ---
    auto divs = doc->getElementsByTagName("div");
    CHECK(divs.size() == 2);
    auto spans = doc->getElementsByTagName("span");
    CHECK(spans.size() == 2);
    auto tds = doc->getElementsByTagName("td");
    CHECK(tds.size() == 4);
    auto everything = doc->getElementsByTagName("*");
    CHECK(everything.size() >= 12);   // html, head, title, body, 2 div, 2 span, table, tbody, 2 tr, 4 td

    // --- query API: querySelector / querySelectorAll (tag, class) ---
    const DOMNode* firstSpan = doc->querySelector("span");
    CHECK(firstSpan != nullptr);
    CHECK(firstSpan->tagName == "span");
    CHECK(textContentOf(firstSpan) == "Hello");

    auto allSpans = doc->querySelectorAll("span.value");
    CHECK(allSpans.size() == 1);
    CHECK(textContentOf(allSpans[0]) == "World");

    // --- query API: compound with id + class ---
    const DOMNode* sel = doc->querySelector("div.box#inner");
    CHECK(sel != nullptr);
    CHECK(sel == inner);

    // --- query API: descendant combinator ---
    const DOMNode* td = doc->querySelector("table#grid td");
    CHECK(td != nullptr);
    CHECK(td->tagName == "td");
    CHECK(textContentOf(td) == "A1");

    auto allTds = doc->querySelectorAll("#grid td");
    CHECK(allTds.size() == 4);

    // --- full-text extraction: ALL text nodes, not just body ---
    std::string full = doc->gatherText();
    // Head text (title) must be present even though it is outside <body>.
    CHECK(full.find("Page Heading") != std::string::npos);
    CHECK(full.find("Hello") != std::string::npos);
    CHECK(full.find("World") != std::string::npos);
    CHECK(full.find("A1") != std::string::npos);
    CHECK(full.find("B2") != std::string::npos);

    // The legacy body-only extraction must NOT see the title text, proving the
    // new full-tree extraction is more complete.
    std::string bodyOnly = bodyOnlyText(doc);
    CHECK(bodyOnly.find("Page Heading") == std::string::npos);
    CHECK(bodyOnly.find("Hello") != std::string::npos);

    delete doc;
}

static void testBead4QueryNoMatch() {
    HTMLParser parser;
    Document* doc = parser.parse("<div id='a'><p class='x'>hi</p></div>");

    // Non-existent id / tag / class → nullptr / empty.
    CHECK(doc->getElementById("nope") == nullptr);
    CHECK(doc->querySelector("#nope") == nullptr);
    CHECK(doc->querySelectorAll(".missing").empty());

    // Class that exists on a different element should not match.
    CHECK(doc->querySelector("p.missing") == nullptr);
    // Child combinator: p is a child of div, so this matches.
    CHECK(doc->querySelector("div > p") != nullptr);
    // Descendant with class.
    CHECK(doc->querySelector("div p.x") != nullptr);

    delete doc;
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
    testManualTreeTraversal();
    testInsertBeforeReplaceRemove();
    testCloneNode();
    testDoctypeCapture();
    testAttributeNodeType();

    testBead2BasicTokenSequence();
    testSelfClosingTags();
    testScriptStyleRawText();
    testMalformedHtml();
    testDoctypeToken();

    testFosterParenting();
    testFosterParentTextInTable();
    testImplicitPClose();
    testComplexHtml();
    testVoidElementsInBody();

    testBead4NestedPageDOM();
    testBead4QueryNoMatch();

    testBrowtest3LargeTable();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
