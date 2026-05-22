/**
 * @file util/String.h
 * @brief String utility functions.
 * @details This module provides helper functions for string manipulation.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <string>

namespace util {

/// @brief Trims whitespace from the beginning and end of a string.
/// @param s The string to trim.
/// @return A trimmed copy of the string.
std::string trim(const std::string& s);

/// @brief Converts a string to lowercase.
/// @param s The string to convert.
/// @return A lowercase copy of the string.
std::string toLower(const std::string& s);

} // namespace util

#endif // UTIL_STRING_H