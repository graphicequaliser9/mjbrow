/**
 * @file CSSParser.cpp
 * @brief CSS3 parser implementation stub.
 * @details Real implementation in bead 4.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "css/CSSParser.h"
#include "css/Selectors.h"
#include "css/ComputedStyle.h"

namespace css {

CSSParser::CSSParser() = default;
CSSParser::~CSSParser() = default;

std::vector<CSSRule> CSSParser::parse(const std::string& /*css*/) {
    return {}; // placeholder – real rules built in bead 4
}

} // namespace css