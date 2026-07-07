/**
 * @file DOMNode.cpp
 * @brief DOM node implementation with efficient memory pool and index-based operations.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "html/DOMNode.h"
#include "html/HTMLParser.h"
#include "css/ComputedStyle.h"
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
        node->style.reset();
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
    node->style.reset();
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
    node->style.reset();
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
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        firstChild->~DOMNode();
        firstChild = next;
    }
    lastChild = nullptr;

    HTMLParser parser;
    Document* newDoc = static_cast<Document*>(parser.parse(html));

    DOMNode* child = newDoc->firstChild;
    while (child) {
        DOMNode* clone = child->cloneNode();
        appendChild(clone);
        child = child->nextSibling;
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
    child->~DOMNode();
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

DOMNode::~DOMNode() {
    while (firstChild) {
        DOMNode* next = firstChild->nextSibling;
        firstChild->~DOMNode();
        firstChild = next;
    }
    firstChild = lastChild = nullptr;
}

DOMNode::DOMNode(const DOMNode& other)
    : nodeType(other.nodeType)
    , parent(nullptr)
    , firstChild(nullptr)
    , lastChild(nullptr)
    , nextSibling(nullptr)
    , prevSibling(nullptr)
    , style(other.style ? std::make_unique<css::ComputedStyle>(*other.style) : nullptr)
    , tagName(other.tagName)
    , attributes(other.attributes)
    , textContent(other.textContent)
    , ownerDocument(other.ownerDocument)
    , namespaceURI(other.namespaceURI)
    , childIndex(0)
    , childCount(0)
{}

DOMNode* DOMNode::cloneNode() const {
    DOMNode* clone = new DOMNode(*this);
    clone->parent = nullptr;
    clone->nextSibling = nullptr;
    clone->prevSibling = nullptr;
    clone->firstChild = nullptr;
    clone->lastChild = nullptr;
    clone->childIndex = 0;
    clone->childCount = 0;

    for (DOMNode* sub = firstChild; sub; sub = sub->nextSibling) {
        DOMNode* subClone = sub->cloneNode();
        clone->appendChild(subClone);
    }

    return clone;
}

Document::Document() {
    nodeType = NodeType::Document;
}

} // namespace html