#include "js/Value.h"
#include <cstring>

namespace js {

Value::Value() : type_(ValueType::Undefined), number_(0) {}

Value::Value(std::nullptr_t) : type_(ValueType::Null), number_(0) {}

Value::Value(bool v) : type_(ValueType::Bool), bool_(v) {}

Value::Value(double v) : type_(ValueType::Number), number_(v) {}

Value::Value(int v) : type_(ValueType::Number), number_(static_cast<double>(v)) {}

Value::Value(const char* v) : type_(ValueType::String), string_(new std::string(v)) {}

Value::Value(const std::string& v) : type_(ValueType::String), string_(new std::string(v)) {}

Value::Value(Object* obj) : type_(ValueType::Object), object_(obj) {}

Value::Value(const Value& other) : type_(other.type_) {
    switch (type_) {
        case ValueType::Undefined:
        case ValueType::Null:
        case ValueType::Bool:
            bool_ = other.bool_;
            break;
        case ValueType::Number:
            number_ = other.number_;
            break;
        case ValueType::String:
            string_ = new std::string(*other.string_);
            break;
        case ValueType::Object:
            object_ = other.object_;
            break;
        default:
            break;
    }
}

Value::Value(Value&& other) noexcept : type_(other.type_) {
    switch (type_) {
        case ValueType::Undefined:
        case ValueType::Null:
        case ValueType::Bool:
            bool_ = other.bool_;
            break;
        case ValueType::Number:
            number_ = other.number_;
            break;
        case ValueType::String:
            string_ = other.string_;
            other.string_ = nullptr;
            break;
        case ValueType::Object:
            object_ = other.object_;
            other.object_ = nullptr;
            break;
        default:
            break;
    }
}

Value::~Value() {
    cleanup();
}

void Value::cleanup() {
    if (type_ == ValueType::String && string_) {
        delete string_;
        string_ = nullptr;
    }
}

Value& Value::operator=(const Value& other) {
    if (this != &other) {
        cleanup();
        type_ = other.type_;
        switch (type_) {
            case ValueType::Undefined:
            case ValueType::Null:
            case ValueType::Bool:
                bool_ = other.bool_;
                break;
            case ValueType::Number:
                number_ = other.number_;
                break;
            case ValueType::String:
                string_ = new std::string(*other.string_);
                break;
            case ValueType::Object:
                object_ = other.object_;
                break;
            default:
                break;
        }
    }
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        cleanup();
        type_ = other.type_;
        switch (type_) {
            case ValueType::Undefined:
            case ValueType::Null:
            case ValueType::Bool:
                bool_ = other.bool_;
                break;
            case ValueType::Number:
                number_ = other.number_;
                break;
            case ValueType::String:
                string_ = other.string_;
                other.string_ = nullptr;
                break;
            case ValueType::Object:
                object_ = other.object_;
                other.object_ = nullptr;
                break;
            default:
                break;
        }
    }
    return *this;
}

Value& Value::operator=(nullptr_t) {
    cleanup();
    type_ = ValueType::Null;
    object_ = nullptr;
    return *this;
}

Value& Value::operator=(bool v) {
    cleanup();
    type_ = ValueType::Bool;
    bool_ = v;
    return *this;
}

Value& Value::operator=(double v) {
    cleanup();
    type_ = ValueType::Number;
    number_ = v;
    return *this;
}

Value& Value::operator=(int v) {
    cleanup();
    type_ = ValueType::Number;
    number_ = static_cast<double>(v);
    return *this;
}

Value& Value::operator=(const char* v) {
    cleanup();
    type_ = ValueType::String;
    string_ = new std::string(v);
    return *this;
}

Value& Value::operator=(const std::string& v) {
    cleanup();
    type_ = ValueType::String;
    string_ = new std::string(v);
    return *this;
}

Value& Value::operator=(Object* obj) {
    cleanup();
    type_ = ValueType::Object;
    object_ = obj;
    return *this;
}

double Value::asNumber() const {
    switch (type_) {
        case ValueType::Number: return number_;
        case ValueType::Bool: return bool_ ? 1 : 0;
        case ValueType::String: return string_ ? std::stod(*string_) : 0;
        default: return 0;
    }
}

bool Value::asBool() const {
    switch (type_) {
        case ValueType::Bool: return bool_;
        case ValueType::Number: return number_ != 0;
        case ValueType::String: return string_ && !string_->empty();
        case ValueType::Null:
        case ValueType::Undefined: return false;
        case ValueType::Object: return object_ != nullptr;
        default: return false;
    }
}

std::string Value::asString() const {
    switch (type_) {
        case ValueType::String: return string_ ? *string_ : "";
        case ValueType::Number: return std::to_string(number_);
        case ValueType::Bool: return bool_ ? "true" : "false";
        case ValueType::Null: return "null";
        case ValueType::Undefined: return "undefined";
        default: return "";
    }
}

Object* Value::asObject() const {
    return (type_ == ValueType::Object) ? object_ : nullptr;
}

// Object implementation
Object::Object() = default;

Object::~Object() = default;

Value Object::get(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second.value;
    }
    // Check prototype chain
    if (prototype_) {
        return prototype_->get(key);
    }
    return Value::undefined();
}

void Object::set(const std::string& key, const Value& value) {
    properties_[key].value = value;
}

bool Object::has(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) return true;
    if (prototype_) return prototype_->has(key);
    return false;
}

void Object::remove(const std::string& key) {
    properties_.erase(key);
}

// Array implementation
Array::Array() {
    static_cast<Object*>(this)->set("length", Value(0));
}

Value Array::get(size_t index) const {
    return Object::get(std::to_string(index));
}

void Array::set(size_t index, const Value& value) {
    Object::set(std::to_string(index), value);
    if (index >= length_) {
        length_ = index + 1;
    }
}

void Array::push(const Value& value) {
    set(length_, value);
    length_++;
}

// Function implementation
Function::Function() = default;

Function::Function(NativeFunction native) : native_(native) {}

Value Function::call(const std::vector<Value>& args) {
    if (native_) {
        return native_(args);
    }
    return Value::undefined();
}

// StringObject implementation
StringObject::StringObject() : value_("") {}
StringObject::StringObject(const std::string& str) : value_(str) {}

// NumberObject implementation
NumberObject::NumberObject() : value_(0) {}
NumberObject::NumberObject(double num) : value_(num) {}

// BooleanObject implementation
BooleanObject::BooleanObject() : value_(false) {}
BooleanObject::BooleanObject(bool b) : value_(b) {}

} // namespace js