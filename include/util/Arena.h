/**
 * @file util/Arena.h
 * @brief Bump-pointer allocator for efficient DOM node memory management.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef UTIL_ARENA_H
#define UTIL_ARENA_H

#include <cstddef>
#include <cstdint>

namespace util {

/**
 * @class ArenaAllocator
 * @brief A bump-pointer allocator for allocating many small objects efficiently.
 * @details Uses a pre-allocated buffer to avoid per-node malloc overhead.
 *          reset() can be called to free all allocations at once (useful for navigation).
 */
class ArenaAllocator {
public:
    ArenaAllocator(size_t initialCapacity = 64 * 1024);
    ~ArenaAllocator();

    void* allocate(size_t size);
    void reset();

    bool owns(void* ptr) const;

private:
    static constexpr size_t CHUNK_SIZE = 64 * 1024;

    struct Chunk {
        uint8_t* data;
        size_t capacity;
        size_t offset;
        Chunk* next;
    };

    Chunk* head_;
    size_t totalAllocated_;
};

} // namespace util

#endif // UTIL_ARENA_H