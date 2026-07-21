#include "layout/BlockLayout.h"

namespace layout {

float verticalMargin(const LayoutNode* node) {
    if (!node) return 0.0f;
    float top = node->marginTop;
    float bottom = node->marginBottom;
    return top + bottom;
}

} // namespace layout
