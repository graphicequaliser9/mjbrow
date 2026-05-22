/**
 * @file util/Memory.h
 * @brief Memory utility functions.
 * @details This module provides helper functions for memory management.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef UTIL_MEMORY_H
#define UTIL_MEMORY_H

#include <cstddef>

namespace util {

/// @brief Safely allocates memory.
/// @param size The size to allocate.
/// @return Pointer to the allocated memory.
void* allocate(size_t size);

/// @brief Safely frees memory.
/// @param ptr Pointer to the memory to free.
void free(void* ptr);

} // namespace util

#endif // UTIL_MEMORY_H