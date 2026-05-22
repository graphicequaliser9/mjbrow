#ifndef JS_DOM_BINDINGS_H
#define JS_DOM_BINDINGS_H

#include "Value.h"
#include <functional>
#include <string>

namespace js {

class DOMElement : public Object {
public:
    DOMElement(const std::string& tag);
    
    std::string tag_name;
    std::string text_content;
    std::string inner_html;
    
    std::unordered_map<std::string, std::string> attributes;
    std::unordered_map<std::string, std::string> styles;
    
    std::vector<std::function<void(DOMElement*)>> event_listeners;
    
    void setAttribute(const std::string& name, const std::string& value);
    void setStyle(const std::string& name, const std::string& value);
};

class HTMLElement : public DOMElement {
public:
    HTMLElement(const std::string& tag);
    
    std::function<void(Event*)> onclick;
    std::function<void(Event*)> onmousedown;
    std::function<void(Event*)> onmouseup;
    std::function<void(Event*)> onmouseover;
    std::function<void(Event*)> onmouseout;
    std::function<void(Event*)> onkeydown;
    std::function<void(Event*)> onkeyup;
    std::function<void(Event*)> onkeypress;
    std::function<void(Event*)> onload;
    std::function<void(Event*)> onerror;
    std::function<void(Event*)> onscroll;
};

class Event {
public:
    std::string type;
    HTMLElement* target = nullptr;
    bool bubbles = false;
    bool cancelable = false;
    bool defaultPrevented = false;
    
    void preventDefault() { defaultPrevented = true; }
};

class Document : public Object {
public:
    Document();
    
    HTMLElement* getElementById(const std::string& id);
    std::vector<HTMLElement*> getElementsByTagName(const std::string& tag);
    HTMLElement* createElement(const std::string& tag);
    
    void addEventListener(const std::string& event, std::function<void(Event*)> callback);
    
    std::vector<HTMLElement*> elements_;
};

class Window : public Object {
public:
    Window();
    
    Document* document = nullptr;
    int innerWidth = 800;
    int innerHeight = 600;
    
    std::function<void()> onload;
    
    void alert(const std::string& msg);
    void console_log(const Value& val);
};

class Navigator : public Object {
public:
    Navigator();
    
    std::string userAgent = "Nitrogen/0.1";
    std::string platform = "Windows";
    std::string language = "en-US";
};

std::vector<Value> bindDOMObjects(Window* window, Document* document, Navigator* navigator);

} // namespace js

#endif // JS_DOM_BINDINGS_H