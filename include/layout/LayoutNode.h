#ifndef LAYOUT_LAYOUTNODE_H
#define LAYOUT_LAYOUTNODE_H

namespace html { class DOMNode; }

namespace layout {

class LayoutNode {
public:
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    bool isBlock{false};
    html::DOMNode* domNode{nullptr};
    LayoutNode* parent{nullptr};
    LayoutNode* firstChild{nullptr};
    LayoutNode* lastChild{nullptr};
    LayoutNode* nextSibling{nullptr};
    LayoutNode* prevSibling{nullptr};
};

} // namespace layout

#endif
