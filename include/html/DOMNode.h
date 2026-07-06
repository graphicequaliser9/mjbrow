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
};

/// @brief Well-known XML namespace URIs used by the tree builder.
inline const char* kNamespaceHTML  = "http://www.w3.org/1999/xhtml";
inline const char* kNamespaceMathML = "http://www.w3.org/1998/Math/MathML";
inline const char* kNamespaceSVG    = "http://www.w3.org/2000/svg";

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
    DOMNode* appendChild(DOMNode* child);

    /// @brief Removes a previously appended child.
    void removeChild(DOMNode* child);

    virtual ~DOMNode();
};

class Document : public DOMNode {
public:
    Document();

    /// @brief Forced-quirks mode as implied by the parsed DOCTYPE.
    bool quirksMode{false};

    /// @brief Name token from the DOCTYPE declaration (e.g. "html").
    std::string doctypeName;

    /// @brief Public / system identifiers from the DOCTYPE (empty if absent).
    std::string doctypePublicId;
    std::string doctypeSystemId;
};

} // namespace html

#endif // HTML_DOMNODE_H