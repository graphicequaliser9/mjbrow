#ifndef JS_VALUE_H
#define JS_VALUE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace js {

class Object;
class Array;
class Function;
class String;
class Number;
class Boolean;

enum class ValueType : uint8_t {
    Undefined,
    Null,
    Bool,
    Number,
    String,
    Object,
    Symbol,
    BigInt
};

class Value {
public:
    Value();
    Value(std::nullptr_t);
    Value(bool v);
    Value(double v);
    Value(int v);
    Value(const char* v);
    Value(const std::string& v);
    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value(Object* obj);
    
    ~Value();
    
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    Value& operator=(nullptr_t);
    Value& operator=(bool v);
    Value& operator=(double v);
    Value& operator=(int v);
    Value& operator=(const char* v);
    Value& operator=(const std::string& v);
    Value& operator=(Object* obj);
    
    ValueType type() const { return type_; }
    
    double asNumber() const;
    bool asBool() const;
    std::string asString() const;
    Object* asObject() const;
    
    bool isUndefined() const { return type_ == ValueType::Undefined; }
    bool isNull() const { return type_ == ValueType::Null; }
    bool isBool() const { return type_ == ValueType::Bool; }
    bool isNumber() const { return type_ == ValueType::Number; }
    bool isString() const { return type_ == ValueType::String; }
    bool isObject() const { return type_ == ValueType::Object; }
    
    static Value undefined() { return Value(); }
    static Value null() { return Value(nullptr); }
    
private:
    ValueType type_ = ValueType::Undefined;
    union {
        double number_;
        bool bool_;
        std::string* string_;
        Object* object_;
    };
    
    void cleanup();
};

class Property {
public:
    std::string key;
    Value value;
    bool enumerable = true;
    bool configurable = true;
    bool writable = true;
    
    Property() = default;
    Property(const std::string& k, const Value& v) : key(k), value(v) {}
};

class Object {
public:
    using PropertyMap = std::unordered_map<std::string, Property>;
    
    Object();
    virtual ~Object();
    
    Value get(const std::string& key) const;
    void set(const std::string& key, const Value& value);
    bool has(const std::string& key) const;
    void remove(const std::string& key);
    
    Object* prototype_ = nullptr;
    
    const PropertyMap& properties() const { return properties_; }
    
protected:
    PropertyMap properties_;
};

class Array : public Object {
public:
    Array();
    
    Value get(size_t index) const;
    void set(size_t index, const Value& value);
    void push(const Value& value);
    size_t length() const { return length_; }
    
private:
    size_t length_ = 0;
};

class Function : public Object {
public:
    using NativeFunction = std::function<Value(const std::vector<Value>&)>;
    
    Function();
    Function(NativeFunction native);
    
    Value call(const std::vector<Value>& args);
    bool isNative() const { return native_ != nullptr; }
    
    void setCode(const std::string& code) { code_ = code; }
    const std::string& code() const { return code_; }
    
private:
    NativeFunction native_;
    std::string code_;
};

class StringObject : public Object {
public:
    StringObject();
    StringObject(const std::string& str);
    std::string value_;
};

class NumberObject : public Object {
public:
    NumberObject();
    NumberObject(double num);
    double value_;
};

class BooleanObject : public Object {
public:
    BooleanObject();
    BooleanObject(bool b);
    bool value_;
};

} // namespace js

#endif // JS_VALUE_H