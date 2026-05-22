#ifndef JS_CONTEXT_H
#define JS_CONTEXT_H

#include "Value.h"
#include "VM.h"
#include <memory>
#include <string>

namespace js {

class JSContext {
public:
    JSContext();
    ~JSContext();
    
    Value evaluate(const std::string& source);
    
    Object* global() const { return global_.get(); }
    
    void setGlobalObject(Object* obj) { global_.reset(obj); }
    
    VM& vm() { return vm_; }
    
    void registerObject(const std::string& name, Object* obj);
    
private:
    std::unique_ptr<Object> global_;
    VM vm_;
    
    void initializeGlobalScope();
};

} // namespace js

#endif // JS_CONTEXT_H