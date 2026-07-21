#ifndef LAYOUT_BLOCKLAYOUT_H
#define LAYOUT_BLOCKLAYOUT_H

#include "layout/LayoutNode.h"
#include "html/DOMNode.h"

namespace layout {

class BlockLayout {
public:
    static LayoutNode* layout(html::DOMNode* doc, int width, int height) {
        (void)doc; (void)width; (void)height;
        return nullptr;
    }
};

float verticalMargin(const LayoutNode* node);

} // namespace layout

#endif
