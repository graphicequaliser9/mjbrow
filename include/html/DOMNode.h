/**
 * @file html/DOMNode.h
 * @brief DOM node types for the HTML parser / layout interface.
 * @details Document, Element, Text, and Comment node types with ownerDocument,
 *          parent / firstChild / lastChild / nextSibling / prevSibling navigation
 *          and a std::map attribute table.
 * @note Shared forward declaration in html/HTMLParser.h removed; include this
 *       header directly where the full DOMNode definition is needed.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef HTML_DOMNODE_H
#define HTML_DOMNODE_H

#include <map>
#include <string>
#include <vector>

// Forward declaration used by ComputedStyle (css/ headers included separately)
namespace css { class ComputedStyle; }

namespace html {

enum class NodeType {
    Document,
    Element,
    Text,
    Comment,
    Attribute,   ///< standalone attribute node (name in tagName, value in textContent)
};

class DOMNode {
public:
    NodeType nodeType{NodeType::Document};
    DOMNode* parent{nullptr};
    DOMNode* firstChild{nullptr};
    DOMNode* lastChild{nullptr};
    DOMNode* nextSibling{nullptr};
    DOMNode* prevSibling{nullptr};
    css::ComputedStyle* style{nullptr};   ///< Computed style set by the cascade pass

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

    // --- innerHTML interface ---

    /// @brief Sets/replaces all children of this node by parsing an HTML string.
    void setInnerHTML(const std::string& html);

    /// @brief Convenience: appends a new child and returns it.
    ///        Takes ownership; if @p child already has a parent it is first
    ///        unlinked from that parent (move semantics).
    DOMNode* appendChild(DOMNode* child);

    /// @brief Inserts @p node immediately before @p child (which must be a child
    ///        of this node). If @p child is null this behaves like appendChild.
    ///        Takes ownership of @p node. Returns the inserted node.
    DOMNode* insertBefore(DOMNode* node, DOMNode* child);

    /// @brief Removes a previously appended child (and deletes it).
    void removeChild(DOMNode* child);

    /// @brief Replaces @p child with @p node, deleting the old @p child.
    ///        Takes ownership of @p node. Returns the inserted node.
    DOMNode* replaceChild(DOMNode* node, DOMNode* child);

    /// @brief Creates a copy of this node. If @p deep is true the entire subtree
    ///        is copied recursively. The returned node is owned by the caller and
    ///        must be deleted.
    DOMNode* cloneNode(bool deep = false) const;

    // --- attribute helpers (Element nodes) ---
    void setAttribute(const std::string& name, const std::string& value);
    const std::string* getAttribute(const std::string& name) const;
    bool hasAttribute(const std::string& name) const;
    void removeAttribute(const std::string& name);
    bool hasAttributes() const { return !attributes.empty(); }
    bool hasChildNodes() const { return firstChild != nullptr; }

    /// @brief Unlinks @p child from this node's child list WITHOUT deleting it.
    ///        Used internally to support move semantics for add/insert/replace.
    void unlink(DOMNode* child);

    ~DOMNode();
};

class Document : public DOMNode {
public:
    Document();

    /// @brief DOCTYPE name (e.g. "html"), empty when no DOCTYPE was seen.
    std::string doctype;
};

} // namespace html

#endif // HTML_DOMNODE_H