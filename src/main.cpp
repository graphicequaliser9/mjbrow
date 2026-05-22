#include <iostream>
#include "js/JSContext.h"
#include "js/Value.h"
#include "js/Parser.h"
#include "js/VM.h"

int main() {
    std::cout << "Nitrogen Browser - ES6 JS Engine Initialized" << std::endl;
    
    js::JSContext context;
    
    // Test basic evaluation
    js::Value result = context.evaluate("1 + 2");
    std::cout << "1 + 2 = " << result.asNumber() << std::endl;
    
    // Test variable declaration
    result = context.evaluate("let x = 42; x");
    std::cout << "let x = 42; x = " << result.asNumber() << std::endl;
    
    // Test string operations
    result = context.evaluate("'hello' + ' ' + 'world'");
    std::cout << "String concat: " << result.asString() << std::endl;
    
    // Test DOM bindings
    result = context.evaluate("typeof window");
    std::cout << "typeof window = " << result.asString() << std::endl;
    
    // Test Math object
    result = context.evaluate("Math.abs(-5)");
    std::cout << "Math.abs(-5) = " << result.asNumber() << std::endl;
    
    std::cout << "JS Engine ready for integration" << std::endl;
    return 0;
}