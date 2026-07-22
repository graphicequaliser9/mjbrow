/**
 * @file Layout.h
 * @brief Layout engine scaffolding.
 * @details This module provides the layout engine interface.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

#include "layout/LayoutNode.h"

namespace html { class DOMNode; }

namespace layout {

class Box {
public:
    Box();
    ~Box();

    static LayoutNode* layout(html::DOMNode* root);

private:
    // Placeholder for internal state
};

} // namespace layout

#endif // LAYOUT_BOX_H