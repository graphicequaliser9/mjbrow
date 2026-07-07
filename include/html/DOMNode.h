/**
 * @file html/DOMNode.h
 * @brief DOM node types for the HTML parser / layout interface.
 * @details Document, Element, Text, and Comment node types with ownerDocument,
 *          parent / firstChild / lastChild / nextSibling / prevSibling navigation
 *          and a std::map attribute table. Uses memory pool with free lists
 *          and index-based child operations for efficient O(1) lookups.
 * @note Shared forward declaration in html/HTMLParser.h removed; include this
 *       header directly where the full DOMNode definition is needed.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_DOMNODE_H
#define HTML_DOMNODE_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward declaration used by ComputedStyle (css/ headers included separately)
namespace css { struct ComputedStyle; }

// Include Arena for DOMNodePool
#include "util/Arena.h"

namespace html {

enum class NodeType {
    Document,
    Element,
    Text,
    Comment,
};

class DOMNode {
public:
    NodeType nodeType{NodeType::Document};
    DOMNode* parent{nullptr};
    DOMNode* firstChild{nullptr};
    DOMNode* lastChild{nullptr};
    DOMNode* nextSibling{nullptr};
    DOMNode* prevSibling{nullptr};
    std::unique_ptr<css::ComputedStyle> style;

    /// @brief Tag name for Element nodes; empty for Document / Text / Comment.
    std::string tagName;

    /// @brief Qualified attributes on this element.
    std::map<std::string, std::string> attributes;

    /// @brief Raw text content (Text / Comment nodes only).
    std::string textContent;

    /// @brief Owning document (null until attached).
    class Document* ownerDocument{nullptr};

    /// @brief Namespace URI ("http://www.w3.org/1999/xhtml" for normal elements).
    std::string namespaceURI;

    /// @brief Stable index for O(1) lookups in parent's child array.
    uint32_t childIndex{0};

    /// @brief Number of children (cached for fast size queries).
    uint32_t childCount{0};

    // --- innerHTML interface ---

    /// @brief Sets/replaces all children of this node by parsing an HTML string.
    void setInnerHTML(const std::string& html);

    /// @brief Convenience: appends a new child and returns it.
    DOMNode* appendChild(DOMNode* child);

    /// @brief Removes a previously appended child.
    void removeChild(DOMNode* child);

    /// @brief Inserts a child before a reference child.
    DOMNode* insertBefore(DOMNode* child, DOMNode* referenceChild);

    /// @brief Gets child at index (O(1) with cached traversal).
    DOMNode* getChildAt(uint32_t index);

    /// @brief Finds child by index using cached first child and index.
    DOMNode* getChildByIndex(uint32_t index);

    ~DOMNode();
    DOMNode() = default;
    DOMNode(const DOMNode& other);
    DOMNode& operator=(const DOMNode& other) = delete;

    /// @brief Recursively clones this node and all descendants.
    DOMNode* cloneNode() const;

private:
    /// @brief Friend class for efficient memory pool access.
    friend class DOMNodePool;
};

class Document : public DOMNode {
public:
    Document();
};

/// @brief Memory pool for DOM nodes with free list reusing instead of malloc/free.
class DOMNodePool {
public:
    DOMNodePool();
    ~DOMNodePool();

    DOMNode* createNode(NodeType type);
    Document* createDocument();

    /// @brief Returns node to free list instead of deallocating.
    void releaseNode(DOMNode* node);

    /// @brief Resets the pool for navigation (frees all nodes).
    void reset();

    /// @brief Gets the underlying arena allocator.
    util::ArenaAllocator& arena() { return arena_; }

private:
    struct FreeNode {
        DOMNode* node;
        FreeNode* next;
    };

    util::ArenaAllocator arena_;
    FreeNode* freeList_{nullptr};  ///< Singly-linked free list of released nodes
    uint32_t totalCreated_{0};     ///< Total nodes created (for stats)
};

} // namespace html

#endif // HTML_DOMNODE_H