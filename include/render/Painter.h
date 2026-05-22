/**
 * @file Painter.h
 * @brief Abstract painting interface.
 * @details Painter is the rendering back-end contract called once per positioned
 *          LayoutNode during the paint pass.  Concrete subclasses (GDI, Direct2D)
 *          are implemented in later beads.  For now this header is the immutable
 *          interface boundary — callers must never bypass it to reach platform APIs.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef RENDER_PAINTER_H
#define RENDER_PAINTER_H

#include <stdint.h>
#include <string>

namespace render {

class Painter {
public:
    virtual ~Painter() = default;

    /// @brief Fills a rectangle with the specified ARGB colour.
    virtual void fillRect(int x, int y, int width, int height, uint32_t color) = 0;

    /// @brief Draws text at the specified baseline location.
    virtual void drawText(int x, int y, const std::string& text, uint32_t color) = 0;

    /// @brief Draws a border around a rectangle.
    virtual void drawBorder(int x, int y, int width, int height, uint32_t color, int thickness) = 0;
};

} // namespace render

#endif // RENDER_PAINTER_H