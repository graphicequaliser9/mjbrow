/**
 * @file ComputedStyle.h
 * @brief ComputedStyle value type holding resolved CSS properties.
 * @details Populated by the cascade engine for each DOM element.  All CSS Display
 *          / Box / Text properties needed by the layout pass are listed so that
 *          the layout engine can read them directly from this struct without
 *          touching the raw CSS declaration map.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef CSS_COMPUTEDSTYLE_H
#define CSS_COMPUTEDSTYLE_H

#include <string>
#include <cstdint>

namespace css {

/**
 * @struct ComputedStyle
 * @brief Fully-resolved style for one element.
 */
struct ComputedStyle {
    // --- display / box model ---
    enum Display { None, Block, Inline, InlineBlock, Flex, Grid, Table, TableRow, TableCell } display{Block};

    enum Position { Static, Relative, Absolute, Fixed, Sticky } position{Static};
    enum Float   { FNone, Left, Right } float_{FNone};

    float width{0.0f};
    float height{0.0f};
    float minWidth{0.0f};
    float maxWidth{0.0f};
    float minHeight{0.0f};
    float maxHeight{0.0f};

    // margin / border / padding – top/right/bottom/left
    float marginTop{0.0f},   marginRight{0.0f}, marginBottom{0.0f}, marginLeft{0.0f};
    float borderTop{0.0f},   borderRight{0.0f}, borderBottom{0.0f}, borderLeft{0.0f};
    float paddingTop{0.0f},  paddingRight{0.0f}, paddingBottom{0.0f}, paddingLeft{0.0f};

    // --- typography ---
    float  fontSize{16.0f};
    std::string fontFamily{"Arial"};
    int    fontWeight{400};
    float  lineHeight{1.2f};
    float  letterSpacing{0.0f};

    // --- colour / background ---
    uint32_t color{0xFF000000};          ///< ARGB
    uint32_t backgroundColor{0x00000000};///< ARGB (transparent)
    float    opacity{1.0f};

    // --- box-sizing ---
    enum BoxSizing { ContentBox, BorderBox } boxSizing{ContentBox};

    // --- border-radius ---
    float borderRadiusTopLeft{0.0f};
    float borderRadiusTopRight{0.0f};
    float borderRadiusBottomRight{0.0f};
    float borderRadiusBottomLeft{0.0f};

    // --- overflow ---
    enum Overflow { Visible, Hidden, Scroll, Auto } overflow{Visible};
};

} // namespace css

#endif // CSS_COMPUTEDSTYLE_H