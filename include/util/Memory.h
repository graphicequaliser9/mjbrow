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

inline void* allocate(size_t size) { return operator new(size); }
inline void free(void* ptr) { operator delete(ptr); }

} // namespace util

#endif // UTIL_MEMORY_H