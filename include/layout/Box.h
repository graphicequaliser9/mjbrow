/**
 * @file Layout.h
 * @brief Layout engine scaffolding.
 * @details This module provides the layout engine interface.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_BOX_H
#define LAYOUT_BOX_H

namespace layout {

class Box {
public:
    Box();
    ~Box();

    void layout(class DOMNode* root);

private:
};

} // namespace layout

#endif // LAYOUT_BOX_H