/**
 * @file DOMNode.cpp
 * @brief DOM node implementation with efficient memory pool and index-based operations.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/DOMNode.h"
#include "html/HTMLParser.h"
#include "util/String.h"
#include <cstdlib>
#include <cstring>

namespace html {

// DOMNodePool implementation
DOMNodePool::DOMNodePool() : freeList_(nullptr), totalCreated_(0) {}

DOMNodePool::~DOMNodePool() {
    reset();
}

DOMNode* DOMNodePool::createNode(NodeType type) {
    totalCreated_++;

    // Check free list first for reuse
    if (freeList_) {
        FreeNode* head = freeList_;
        freeList_ = freeList_->next;

        // Reinitialize the node
        DOMNode* node = head->node;
        node->nodeType = type;
        node->parent = nullptr;
        node->firstChild = nullptr;
        node->lastChild = nullptr;
        node->nextSibling = nullptr;
        node->prevSibling = nullptr;
        node->style = nullptr;
        node->ownerDocument = nullptr;
        node->childIndex = 0;
        node->childCount = 0;
        node->tagName.clear();
        node->attributes.clear();
        node->textContent.clear();
        node->namespaceURI.clear();

        // Free the FreeNode wrapper
        free(head);
        return node;
    }

    // Allocate new node from arena
    void* ptr = arena_.allocate(sizeof(DOMNode));
    DOMNode* node = new (ptr) DOMNode();
    node->nodeType = type;
    node->parent = nullptr;
    node->firstChild = nullptr;
    node->lastChild = nullptr;
    node->nextSibling = nullptr;
    node->prevSibling = nullptr;
    node->style = nullptr;
    node->ownerDocument = nullptr;
    node->childIndex = 0;
    node->childCount = 0;
    return node;
}

Document* DOMNodePool::createDocument() {
    void* ptr = arena_.allocate(sizeof(Document));
    Document* doc = new (ptr) Document();
    return doc;
}

void DOMNodePool::releaseNode(DOMNode* node) {
    // Add to free list instead of deallocating
    FreeNode* freeNode = static_cast<FreeNode*>(malloc(sizeof(FreeNode)));
    freeNode->node = node;
    freeNode->next = freeList_;
    freeList_ = freeNode;

    // Reset pointers for reuse
    node->parent = nullptr;
    node->firstChild = nullptr;
    node->lastChild = nullptr;
    node->nextSibling = nullptr;
    node->prevSibling = nullptr;
    node->style = nullptr;
    node->ownerDocument = nullptr;
}

void DOMNodePool::reset() {
    // Clean up free list nodes
    FreeNode* current = freeList_;
    while (current) {
        FreeNode* next = current->next;
        free(current);
        current = next;
    }
    freeList_ = nullptr;

    // Reset arena
    arena_.reset();
}

// DOMNode implementation
void DOMNode::setInnerHTML(const std::string& html) {
    // Note: This creates a temporary parser with its own pool for parsing.
    // In production, we'd want to pass the pool or use a different approach
    // to avoid cross-pool memory issues.
    HTMLParser parser;
    Document* newDoc = static_cast<Document*>(parser.parse(html));

    // Clear existing children
    while (firstChild) {
        DOMNode* child = firstChild;
        removeChild(child);
    }

    // Move children from parsed document
    DOMNode* child = newDoc->firstChild;
    while (child) {
        DOMNode* next = child->nextSibling;
        // Detach from source document
        if (child->parent) {
            child->parent->removeChild(child);
        }
        // Append to this node (reuses the node, doesn't copy)
        appendChild(child);
        child = next;
    }
}

DOMNode* DOMNode::appendChild(DOMNode* child) {
    if (!child || child->parent == this) {
        return child;
    }

    // Detach from previous parent
    if (child->parent) {
        child->parent->removeChild(child);
    }

    child->parent = this;
    child->prevSibling = lastChild;
    child->childIndex = childCount;

    if (lastChild) {
        lastChild->nextSibling = child;
    }
    lastChild = child;
    if (!firstChild) {
        firstChild = child;
    }

    childCount++;

    // Set owner document
    DOMNode* p = this;
    while (p && p->nodeType != NodeType::Document) {
        p = p->parent;
    }
    if (p) {
        child->ownerDocument = static_cast<Document*>(p);
    }

    // Update child indices of siblings after this one
    if (child->nextSibling) {
        DOMNode* sibling = child->nextSibling;
        uint32_t idx = child->childIndex + 1;
        while (sibling) {
            sibling->childIndex = idx++;
            sibling = sibling->nextSibling;
        }
    }

    return child;
}

void DOMNode::removeChild(DOMNode* child) {
    if (!child || child->parent != this) {
        return;
    }

    if (child->prevSibling) {
        child->prevSibling->nextSibling = child->nextSibling;
    } else {
        firstChild = child->nextSibling;
    }

    if (child->nextSibling) {
        child->nextSibling->prevSibling = child->prevSibling;
        // Update child indices of subsequent siblings
        DOMNode* sibling = child->nextSibling;
        uint32_t idx = child->childIndex;
        while (sibling) {
            sibling->childIndex = idx++;
            sibling = sibling->nextSibling;
        }
    } else {
        lastChild = child->prevSibling;
    }

    child->parent = nullptr;
    child->prevSibling = nullptr;
    child->nextSibling = nullptr;
    child->childIndex = 0;
    childCount--;
}

DOMNode* DOMNode::insertBefore(DOMNode* child, DOMNode* referenceChild) {
    if (!child || child->parent == this) {
        return child;
    }

    // Handle insert at end (same as appendChild)
    if (!referenceChild) {
        return appendChild(child);
    }

    // Ensure referenceChild is actually our child
    if (referenceChild->parent != this) {
        return child;
    }

    // Detach from previous parent
    if (child->parent) {
        child->parent->removeChild(child);
    }

    child->parent = this;
    child->childIndex = referenceChild->childIndex;

    // Link into the list
    child->prevSibling = referenceChild->prevSibling;
    child->nextSibling = referenceChild;

    if (referenceChild->prevSibling) {
        referenceChild->prevSibling->nextSibling = child;
    } else {
        firstChild = child;
    }

    referenceChild->prevSibling = child;

    // Update child indices
    DOMNode* sibling = child->prevSibling;
    uint32_t minIndex = child->childIndex;

    // Update indices of all siblings from this position forward
    DOMNode* curr = firstChild;
    uint32_t idx = 0;
    while (curr) {
        curr->childIndex = idx++;
        curr = curr->nextSibling;
    }

    childCount++;
    return child;
}

DOMNode* DOMNode::getChildAt(uint32_t index) {
    // Fast path using index - O(n) but with direct traversal
    DOMNode* child = firstChild;
    while (child && child->childIndex < index) {
        child = child->nextSibling;
    }
    if (child && child->childIndex == index) {
        return child;
    }
    return nullptr;
}

DOMNode* DOMNode::getChildByIndex(uint32_t index) {
    // Optimized traversal using cached childIndex
    if (index >= childCount) {
        return nullptr;
    }

    DOMNode* child = firstChild;
    while (child) {
        if (child->childIndex == index) {
            return child;
        }
        child = child->nextSibling;
    }
    return nullptr;
}

DOMNode::~DOMNode() = default;

Document::Document() {
    nodeType = NodeType::Document;
}

} // namespace html