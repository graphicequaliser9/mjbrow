/**
 * @file TextMeasurer.cpp
 * @brief Cross-platform text measurement implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "layout/TextMeasurer.h"

#include <cmath>
#include <algorithm>

namespace layout {

TextMeasurer::TextMeasurer() = default;
TextMeasurer::~TextMeasurer() = default;

TextMeasurer::Metrics TextMeasurer::measure(const std::string& text, float fontSize,
                                            const std::string& fontFamily) {
    Metrics m = measureFallback(text, fontSize, fontFamily);
    m.height = lineHeight(fontSize, 1.2f);
    return m;
}

float TextMeasurer::lineHeight(float fontSize, float cssLineHeight) const {
    if (cssLineHeight > 0.0f) {
        if (cssLineHeight < 1.0f) {
            return fontSize * cssLineHeight;
        }
        if (cssLineHeight < 10.0f) {
            return fontSize * cssLineHeight;
        }
        return cssLineHeight;
    }
    return fontSize * 1.2f;
}

TextMeasurer::Metrics TextMeasurer::measureFallback(const std::string& text, float fontSize,
                                                    const std::string& fontFamily) {
    Metrics m;
    m.ascent = fontSize * 0.8f;
    m.descent = fontSize * 0.2f;
    float charWidth = fontSize * 0.5f;
    if (fontFamily.find("serif") != std::string::npos) {
        charWidth = fontSize * 0.55f;
    } else if (fontFamily.find("mono") != std::string::npos) {
        charWidth = fontSize * 0.6f;
    } else if (fontFamily.find("Arial") != std::string::npos || fontFamily.find("sans") != std::string::npos) {
        charWidth = fontSize * 0.5f;
    }
    m.width = static_cast<float>(text.size()) * charWidth;
    return m;
}

} // namespace layout
