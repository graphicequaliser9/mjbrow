#include "js/Heap.h"
#include "js/GC.h"

namespace js {

Heap::Heap() : Heap(DEFAULT_HEAP_SIZE) {}

Heap::Heap(size_t size) 
    : memory_(std::make_unique<uint8_t[]>(size)), capacity_(size) {}

Heap::~Heap() = default;

void Heap::triggerGC() {
    // Simple GC trigger - in production would be more sophisticated
    // For now, just reset and continue
    reset();
}

void Heap::collect() {
    // Mark and sweep would be coordinated with GC class
    // Heap provides memory management, GC handles marking
}

void Heap::reset() {
    offset_ = 0;
    allocations_.clear();
}

} // namespace js