/**
 * @file TextMeasurer.h
 * @brief Font metrics and text shaping interface.
 * @details Provides cross-platform text measurement.  On Windows this wraps
 *          DirectWrite / GDI; on other platforms a heuristic fallback is used.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef LAYOUT_TEXTMEASURER_H
#define LAYOUT_TEXTMEASURER_H

#include <string>

namespace layout {

class TextMeasurer {
public:
    struct Metrics {
        float width{0.0f};
        float height{0.0f};
        float ascent{0.0f};
        float descent{0.0f};
    };

    TextMeasurer();
    ~TextMeasurer();

    Metrics measure(const std::string& text, float fontSize, const std::string& fontFamily);
    float lineHeight(float fontSize, float cssLineHeight) const;

private:
    Metrics measureFallback(const std::string& text, float fontSize, const std::string& fontFamily);
};

} // namespace layout

#endif // LAYOUT_TEXTMEASURER_H
