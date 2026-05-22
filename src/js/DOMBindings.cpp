#include "js/DOMBindings.h"

namespace js {

// DOMElement implementation
DOMElement::DOMElement(const std::string& tag) : tag_name(tag) {}

void DOMElement::setAttribute(const std::string& name, const std::string& value) {
    attributes[name] = value;
}

void DOMElement::setStyle(const std::string& name, const std::string& value) {
    styles[name] = value;
}

// HTMLElement implementation
HTMLElement::HTMLElement(const std::string& tag) : DOMElement(tag) {}

// Document implementation
Document::Document() = default;

HTMLElement* Document::getElementById(const std::string& id) {
    for (auto* el : elements_) {
        auto it = el->attributes.find("id");
        if (it != el->attributes.end() && it->second == id) {
            return el;
        }
    }
    return nullptr;
}

std::vector<HTMLElement*> Document::getElementsByTagName(const std::string& tag) {
    std::vector<HTMLElement*> result;
    for (auto* el : elements_) {
        if (el->tag_name == tag) {
            result.push_back(el);
        }
    }
    return result;
}

HTMLElement* Document::createElement(const std::string& tag) {
    auto* el = new HTMLElement(tag);
    elements_.push_back(el);
    return el;
}

void Document::addEventListener(const std::string& event, std::function<void(Event*)> callback) {
    // Store event listener
    (void)event;
    (void)callback;
}

// Window implementation
Window::Window() {
    document = new Document();
}

void Window::alert(const std::string& msg) {
    // Platform-specific alert dialog
    (void)msg;
}

void Window::console_log(const Value& val) {
    // Output to console
    (void)val;
}

// Navigator implementation
Navigator::Navigator() = default;

// Helper function to bind DOM objects
std::vector<Value> bindDOMObjects(Window* window, Document* document, Navigator* navigator) {
    std::vector<Value> result;
    result.push_back(Value(window));
    result.push_back(Value(document));
    result.push_back(Value(navigator));
    return result;
}

} // namespace js