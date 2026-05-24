/**
 * @file Memory.cpp
 * @brief Memory utility stub.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "util/Memory.h"

namespace util {

void* allocate(size_t /*size*/) {
    return nullptr; // placeholder – real allocator in js/Heap in bead 7
}

void free(void* /*ptr*/) {
    // placeholder
}

} // namespace util