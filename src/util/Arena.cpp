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
    , tail_(nullptr)
    , totalAllocated_(0)
    , totalCapacity_(0) {
    head_ = new Chunk();
    head_->capacity = initialCapacity;
    head_->offset = 0;
    head_->next = nullptr;
    head_->data = static_cast<uint8_t*>(malloc(initialCapacity));
    if (head_->data) {
        totalCapacity_ = initialCapacity;
    }
    tail_ = head_;
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

    Chunk* current = tail_;
    while (current) {
        if (current->offset + alignedSize <= current->capacity) {
            void* ptr = current->data + current->offset;
            current->offset += alignedSize;
            totalAllocated_ += alignedSize;
            return ptr;
        }
        current = current->next;
        if (current) {
            tail_ = current;
        }
    }

    // Need a new chunk
    Chunk* chunk = new Chunk();
    chunk->capacity = CHUNK_SIZE;
    chunk->offset = alignedSize;
    chunk->data = static_cast<uint8_t*>(malloc(CHUNK_SIZE));
    chunk->next = nullptr;

    if (!head_) {
        head_ = chunk;
    } else {
        tail_->next = chunk;
    }
    tail_ = chunk;
    totalCapacity_ += CHUNK_SIZE;

    totalAllocated_ += alignedSize;
    return chunk->data;
}

void ArenaAllocator::reset() {
    Chunk* current = head_;
    while (current) {
        current->offset = 0;
        current = current->next;
    }
    tail_ = head_;
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

size_t ArenaAllocator::capacityRemaining() const {
    if (!tail_) return 0;
    return tail_->capacity - tail_->offset;
}

size_t ArenaAllocator::totalCapacity() const {
    return totalCapacity_;
}

} // namespace util