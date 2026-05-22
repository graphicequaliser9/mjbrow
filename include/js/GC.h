#ifndef JS_GC_H
#define JS_GC_H

#include "Value.h"
#include "Heap.h"
#include <vector>
#include <unordered_set>

namespace js {

class GC {
public:
    GC();
    
    void collect();
    void mark(Value& value);
    void mark(Object* obj);
    
    void setRoot(Value* root) { roots_.push_back(root); }
    void clearRoots() { roots_.clear(); }
    
    void setHeap(Heap* heap) { heap_ = heap; }
    
    bool isMarked(Object* obj) const;
    void markObject(Object* obj);
    
private:
    Heap* heap_ = nullptr;
    std::vector<Value*> roots_;
    std::unordered_set<Object*> marked_;
    
    void sweep();
    void markAll();
};

} // namespace js

#endif // JS_GC_H