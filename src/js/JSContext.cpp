#include "js/JSContext.h"
#include "js/DOMBindings.h"

namespace js {

JSContext::JSContext() {
    global_ = std::make_unique<Object>();
    vm_.setCurrentContext(this);
    initializeGlobalScope();
}

JSContext::~JSContext() = default;

Value JSContext::evaluate(const std::string& source) {
    vm_.setCurrentContext(this);
    return vm_.execute(source);
}

void JSContext::registerObject(const std::string& name, Object* obj) {
    global_->set(name, Value(obj));
}

void JSContext::initializeGlobalScope() {
    // Create DOM objects
    auto window = new Window();
    auto document = new Document();
    auto navigator = new Navigator();
    
    window->document = document;
    
    registerObject("window", window);
    registerObject("document", document);
    registerObject("navigator", navigator);
    
    // Math object
    auto math = new Object();
    auto math_abs = new Function([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0);
        return Value(std::abs(args[0].asNumber()));
    });
    math->set("abs", Value(math_abs));
    registerObject("Math", math);
    
    // console object
    auto console = new Object();
    auto console_log = new Function([](const std::vector<Value>& args) -> Value {
        for (const auto& arg : args) {
            // Output to debug console
        }
        return Value::undefined();
    });
    console->set("log", Value(console_log));
    registerObject("console", console);
}

} // namespace js