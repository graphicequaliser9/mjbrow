/**
 * @file DOMInspector.h
 * @brief DOM tree-view inspector panel.
 * @details Tree-view of the live DOM, node selection, computed-style panel,
 *          and a repaint-bounding-box highlight overlay sourced from the
 *          layout engine.  Works on any platform; the overlay is rendered by
 *          the BrowserUI chrome layer.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef DEVTOOLS_DOMINSPECTOR_H
#define DEVTOOLS_DOMINSPECTOR_H

#include <string>
#include <vector>
#include <memory>

// Forward declaration – DOMNode has its own header (html/DOMNode.h) that
// drags in css/ComputedStyle.h; we keep the inspector header dependency-light.
namespace html {
    class DOMNode;
}

namespace devtools {

/**
 * @struct StyleProperty
 * @brief One resolved CSS property for the computed-style panel.
 */
struct StyleProperty {
    std::string name;      ///< CSS property name (e.g. "display")
    std::string value;     ///< Resolved value (e.g. "block")
};

/**
 * @struct InspectorNode
 * @brief Lightweight tree node used by the DOMInspector tree-view.
 * @details One InspectorNode wraps a live html::DOMNode pointer so the
 *          inspector tree can be traversed without destroying the real DOM.
 */
struct InspectorNode {
    html::DOMNode* domNode;           ///< Live DOM node (not owned)
    std::vector<std::unique_ptr<InspectorNode>> children; ///< Expanded children
    bool expanded{false};             ///< True → children are populated
    bool hasChildren{false};          ///< True even when collapsed (shows chevron)
};

/**
 * @struct OverlayRect
 * @brief Rectangle drawn on top of the page canvas during hover.
 * @details Emitted by the inspector so BrowserUI can redraw a semi-transparent
 *          highlight over the node's paint bounding box.
 */
struct OverlayRect {
    int x, y, width, height;  ///< Viewport coords, pixels
    uint32_t color;           ///< ARGB fill colour (semi-transparent by default)
};

/**
 * @class DOMInspector
 * @brief DOM tree-view inspector with node selection and computed-style panel.
 */
class DOMInspector {
public:
    DOMInspector();
    ~DOMInspector();

    /**
     * @brief Attaches to a live DOM document and builds the initial tree root.
     * @param rootDoc The document node to inspect.
     */
    void attach(html::DOMNode* rootDoc);

    /**
     * @brief Rebuilds collapsed child nodes for a given node.
     * @param node The parent to expand.
     */
    void expandNode(InspectorNode& node);

    /**
     * @brief Collapses a node (clears its children vector, keeps the node).
     * @param node The parent to collapse.
     */
    void collapseNode(InspectorNode& node);

    /**
     * @brief Select a node by index into the flat tree and populate the style panel.
     * @param flatIndex The zero-based index as produced by flattenTree.
     * @return The selected InspectorNode, or nullptr on invalid index.
     */
    InspectorNode* selectByFlatIndex(size_t flatIndex);

    /**
     * @brief Returns a flat depth-first listing of the current tree state.
     * @return Flat list of InspectorNode pointers (shallow, owner remains the tree).
     */
    std::vector<InspectorNode*> flattenTree() const;

    /**
     * @brief Returns the computed styles for the currently selected node.
     * @return List of name/value style properties; empty if nothing selected.
     */
    std::vector<StyleProperty> getSelectedComputedStyles() const;

    /**
     * @brief Sets hover feedback; returns an overlay rect for the hovered node.
     * @param hoveredNode The node currently under the pointer (nullptr to clear).
     * @return Overlay rectangle – empty if no hover rect is available.
     */
    OverlayRect getHoverOverlay(InspectorNode* hoveredNode) const;

    /**
     * @brief Returns the currently selected node (or nullptr).
     */
    InspectorNode* getSelectedNode() const { return selectedNode_; }

    /**
     * @brief Returns the root tree node.  Rebuilds it if document changed.
     */
    InspectorNode* getRoot() const { return root_.get(); }

    /**
     * @brief Whether DevTools are currently visible and active.
     */
    bool isVisible() const { return visible_; }

    /**
     * @brief Toggle  visibility of the inspector.
     */
    void setVisible(bool v) { visible_ = v; }

private:
    /**
     * @brief Recursively builds the full expanded tree from a live DOM node.
     * @param domNode The live DOM node to wrap.
     * @return Owned InspectorNode tree rooted at domNode.
     */
    std::unique_ptr<InspectorNode> buildTreeRecursive(html::DOMNode* domNode);

    html::DOMNode* documentRoot_{nullptr};         ///< Live document pointer
    std::unique_ptr<InspectorNode> root_;          ///< Root of inspector tree
    InspectorNode* selectedNode_{nullptr};          ///< Currently selected tree node
    bool visible_{false};                           ///< UI visibility flag
};

} // namespace devtools

#endif // DEVTOOLS_DOMINSPECTOR_H
