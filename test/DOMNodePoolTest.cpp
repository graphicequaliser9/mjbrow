/**
 * @file DOMNodePoolTest.cpp
 * @brief Unit tests for DOMNodePool memory management and cloneNode.
 * @copyright 2026, Nitrogen Browser Project
 */

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
    std::printf("DOMNodePool tests\n");

    html::DOMNodePool pool;

    // Test 1: Pool creates nodes
    html::DOMNode* node = pool.createNode(html::NodeType::Element);
    check(node != nullptr, "pool creates a node");
    check(node->nodeType == html::NodeType::Element, "node type is correct");
    check(node->firstChild == nullptr, "new node has no children");

    // Test 2: Pool reset clears nodes
    pool.reset();
    html::DOMNode* node2 = pool.createNode(html::NodeType::Text);
    check(node2 != nullptr, "pool creates node after reset");

    // Test 3: cloneNode deep-copies subtree
    html::DOMNode* parent = pool.createNode(html::NodeType::Element);
    parent->tagName = "ul";
    html::DOMNode* child1 = pool.createNode(html::NodeType::Element);
    child1->tagName = "li";
    html::DOMNode* child2 = pool.createNode(html::NodeType::Element);
    child2->tagName = "li";
    parent->appendChild(child1);
    parent->appendChild(child2);

    html::DOMNode* clone = parent->cloneNode();
    check(clone != nullptr, "cloneNode returns non-null");
    check(clone->tagName == "ul", "clone has same tagName");
    check(clone->childCount == 2, "clone has same child count");
    check(clone->firstChild != nullptr, "clone has first child");
    check(clone->firstChild->nextSibling != nullptr, "clone children are linked");

    delete clone;

    if (g_failures == 0) {
        std::printf("All DOMNodePool tests passed.\n");
        return 0;
    }
    std::printf("%d DOMNodePool test(s) FAILED.\n", g_failures);
    return 1;
}
