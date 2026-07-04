/**
 * @file Arena.cpp
 * @brief Bump-pointer allocator implementation.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "util/Arena.h"
#include <cstdlib>
#include <cstring>

namespace util {

ArenaAllocator::ArenaAllocator(size_t initialCapacity)
    : head_(nullptr)
    , totalAllocated_(0) {
    head_ = new Chunk();
    head_->capacity = initialCapacity;
    head_->offset = 0;
    head_->next = nullptr;
    head_->data = static_cast<uint8_t*>(malloc(initialCapacity));
}

ArenaAllocator::~ArenaAllocator() {
    Chunk* current = head_;
    while (current) {
        Chunk* next = current->next;
        free(current->data);
        delete current;
        current = next;
    }
}

void* ArenaAllocator::allocate(size_t size) {
    // Align to 8 bytes for proper alignment
    size_t alignedSize = (size + 7) & ~static_cast<size_t>(7);

    if (!head_ || head_->offset + alignedSize > head_->capacity) {
        Chunk* chunk = new Chunk();
        chunk->capacity = CHUNK_SIZE;
        chunk->offset = 0;
        chunk->data = static_cast<uint8_t*>(malloc(CHUNK_SIZE));
        chunk->next = head_;
        head_ = chunk;
    }

    void* ptr = head_->data + head_->offset;
    head_->offset += alignedSize;
    totalAllocated_ += alignedSize;
    return ptr;
}

void ArenaAllocator::reset() {
    // Free all chunks except the first one
    Chunk* current = head_;
    while (current && current->next) {
        current->offset = 0;
        current = current->next;
    }
    if (head_) {
        head_->offset = 0;
    }
    totalAllocated_ = 0;
}

bool ArenaAllocator::owns(void* ptr) const {
    Chunk* current = head_;
    while (current) {
        if (ptr >= current->data && ptr < current->data + current->offset) {
            return true;
        }
        current = current->next;
    }
    return false;
}

} // namespace util