/**
 * @file HTMLParserTest.cpp
 * @brief Unit tests for HTMLParser tokenizer, tree builder, and deeply nested HTML.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/HTMLParser.h"
#include "html/DOMNode.h"

#include <cstdio>
#include <cstring>
#include <functional>

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
    std::printf("HTMLParser tests\n");

    html::HTMLParser parser;

    // Test 1: Basic HTML parsing
    {
        html::DOMNode* doc = parser.parse("<html><body><p>Hello</p></body></html>");
        check(doc != nullptr, "parse returns non-null document");
        if (doc) {
            bool foundP = false;
            std::function<void(html::DOMNode*)> walk = [&](html::DOMNode* node) {
                if (!node) return;
                if (node->nodeType == html::NodeType::Element && node->tagName == "p") {
                    foundP = true;
                }
                for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
                    walk(child);
                }
            };
            walk(doc);
            check(foundP, "found <p> element in parsed tree");
        }
    }

    // Test 2: Deeply nested HTML does not overflow stack
    {
        std::string deep;
        deep.reserve(20000);
        deep += "<div>";
        for (int i = 0; i < 500; ++i) {
            deep += "<span>";
        }
        deep += "deep";
        for (int i = 0; i < 500; ++i) {
            deep += "</span>";
        }
        deep += "</div>";

        html::DOMNode* doc = parser.parse(deep);
        check(doc != nullptr, "deeply nested HTML parses without crash");
        if (doc) {
            int depth = 0;
            for (html::DOMNode* child = doc->firstChild; child; child = child->nextSibling) {
                if (child->nodeType == html::NodeType::Element && child->tagName == "div") {
                    depth = 1;
                    for (html::DOMNode* span = child->firstChild; span; span = span->firstChild) {
                        if (span->nodeType == html::NodeType::Element && span->tagName == "span") {
                            depth++;
                        } else {
                            break;
                        }
                    }
                    break;
                }
            }
            char msg[128];
            std::snprintf(msg, sizeof(msg), "deep nesting depth is correct (got %d)", depth);
            check(depth == 501, msg);
        }
    }

    // Test 3: setInnerHTML clones nodes (no use-after-free)
    {
        html::DOMNode* doc = parser.parse("<div><p>Original</p></div>");
        check(doc != nullptr, "parse for setInnerHTML test");
        if (doc && doc->firstChild) {
            html::DOMNode* div = doc->firstChild;
            div->setInnerHTML("<span>Cloned</span>");
            bool foundSpan = false;
            bool foundP = false;
            for (html::DOMNode* child = div->firstChild; child; child = child->nextSibling) {
                if (child->nodeType == html::NodeType::Element && child->tagName == "span") {
                    foundSpan = true;
                }
                if (child->nodeType == html::NodeType::Element && child->tagName == "p") {
                    foundP = true;
                }
            }
            check(foundSpan, "setInnerHTML adds new children");
            check(!foundP, "setInnerHTML removes old children");
        }
    }

    if (g_failures == 0) {
        std::printf("All HTMLParser tests passed.\n");
        return 0;
    }
    std::printf("%d HTMLParser test(s) FAILED.\n", g_failures);
    return 1;
}
