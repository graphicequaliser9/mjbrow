#ifndef JS_HEAP_H
#define JS_HEAP_H

#include "Value.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>

namespace js {

class Heap {
public:
    static constexpr size_t DEFAULT_HEAP_SIZE = 1024 * 1024 * 4; // 4 MB
    static constexpr size_t ALLOCATION_THRESHOLD = DEFAULT_HEAP_SIZE / 2;
    
    Heap();
    explicit Heap(size_t size);
    ~Heap();
    
    void* allocate(size_t size);
    template<typename T, typename... Args>
    T* construct(Args&&... args);
    
    void collect();
    void reset();
    
    size_t used() const { return offset_; }
    size_t capacity() const { return capacity_; }
    
private:
    std::unique_ptr<uint8_t[]> memory_;
    size_t offset_ = 0;
    size_t capacity_ = 0;
    std::vector<void*> allocations_;
    
    void triggerGC();
};

inline void* Heap::allocate(size_t size) {
    size_t aligned = (size + 7) & ~7; // 8-byte alignment
    
    if (offset_ + aligned > capacity_) {
        triggerGC();
        if (offset_ + aligned > capacity_) {
            // Still can't allocate, grow the heap
            // For simplicity, throw or handle gracefully
            return nullptr;
        }
    }
    
    void* ptr = memory_.get() + offset_;
    offset_ += aligned;
    return ptr;
}

template<typename T, typename... Args>
T* Heap::construct(Args&&... args) {
    void* memory = allocate(sizeof(T));
    if (!memory) return nullptr;
    
    T* obj = new(memory) T(std::forward<Args>(args)...);
    allocations_.push_back(obj);
    return obj;
}

} // namespace js

#endif // JS_HEAP_H