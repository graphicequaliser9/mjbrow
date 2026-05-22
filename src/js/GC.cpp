#include "js/GC.h"

namespace js {

GC::GC() = default;

void GC::collect() {
    markAll();
    sweep();
}

void GC::mark(Value& value) {
    if (value.isObject()) {
        markObject(value.asObject());
    }
}

void GC::markObject(Object* obj) {
    if (obj) {
        marked_.insert(obj);
        // Mark prototype
        if (obj->prototype_) {
            markObject(obj->prototype_);
        }
        // Mark property values
        for (const auto& [key, prop] : obj->properties()) {
            if (prop.value.isObject()) {
                markObject(prop.value.asObject());
            }
        }
    }
}

bool GC::isMarked(Object* obj) const {
    return marked_.find(obj) != marked_.end();
}

void GC::markAll() {
    for (Value* root : roots_) {
        mark(*root);
    }
}

void GC::sweep() {
    // Sweep is handled by heap - this is a marking GC
    marked_.clear();
}

} // namespace js