/**
 * @file Tab.cpp
 * @brief Tab implementation: navigation, frame tick, paint/layout/JS stub pipeline.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "browser/Tab.h"
#include "browser/SettingsPanel.h"
#include "browser/URLBar.h"

#include "html/HTMLParser.h"
#include "html/DOMNode.h"
#include "net/HttpClient.h"
#include "js/VM.h"
#include "js/QuickJS.h"
#include "css/CSSParser.h"
#include "css/Cascade.h"
#include "layout/Box.h"
#include "layout/TextMeasurer.h"
#include "util/Logging.h"
#include "util/String.h"

#include <algorithm>
#include <chrono>

namespace browser {

Tab::Tab() {
    initJS();
}

Tab::Tab(const std::string& initialUrl) {
    initJS();
    if (!initialUrl.empty()) navigate(initialUrl);
}

Tab::~Tab() {
    if (js_) quickjs::JS_FreeContext(js_);
    if (rt_) quickjs::JS_FreeRuntime(rt_);
}

static std::string extractTextContent(html::DOMNode* node) {
    std::string result;
    if (!node) return result;
    for (html::DOMNode* child = node->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Text) {
            result += child->textContent;
        } else if (child->nodeType == html::NodeType::Element) {
            result += extractTextContent(child);
        }
    }
    return result;
}

void Tab::navigate(const std::string& url) {
    url_     = url;
    loading_ = true;

    util::Log(util::LogLevel::Info, "Tab::navigate → " + url + "\n");

    net::HttpClient client;
    auto response   = client.sendRequest(url, "GET");
    std::string body(response.body.begin(), response.body.end());
    rawHtml_ = body;

    if (rawHtml_.empty()) {
        util::Log(util::LogLevel::Warn, "Tab: empty response from " + url + "\n");
        bodyText_ = "(failed to load content)";
        loading_ = false;
        return;
    }

    parseHTML(rawHtml_);

    html::DOMNode* bodyNode = nullptr;
    if (document_) {
        for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
            if (child->nodeType == html::NodeType::Element && child->tagName == "body") {
                bodyNode = child;
                break;
            }
        }
    }
    bodyText_ = extractTextContent(bodyNode);

    cascadeStyles();
    performLayout();
    runScripts();
    loading_ = false;
}

void Tab::loadHTML(const std::string& html) {
    loading_ = true;
    rawHtml_ = html;

    // Build the full DOM tree with the HTML5 parser (Beads 1-3 pipeline).
    parseHTML(rawHtml_);

    // Text extraction walks the *entire* DOM tree, not just <body>, so text
    // in the head, in misnested fragments, and in deeply nested elements is
    // all captured.
    html::DOMNode* bodyNode = nullptr;
    if (document_) {
        for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
            if (child->nodeType == html::NodeType::Element && child->tagName == "body") {
                bodyNode = child;
                break;
            }
        }
    }
    bodyText_ = extractTextContent(bodyNode);

    cascadeStyles();
    performLayout();
    runScripts();
    loading_ = false;
}

void Tab::tick(double dtMs) {
    (void)dtMs;
    if (loading_ || !document_) return;

    // Drive the JS runtime: run microtasks/pending jobs and fire RAF callbacks.
    runJSEventLoop();

    paintFrame();
}

std::string Tab::title() const {
    if (!document_) return url_;
    for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "title") {
            std::string result;
            for (html::DOMNode* textChild = child->firstChild; textChild; textChild = textChild->nextSibling) {
                if (textChild->nodeType == html::NodeType::Text) {
                    result += textChild->textContent;
                }
            }
            return result;
        }
    }
    return url_;
}

void Tab::clear() {
    document_.reset();
    vm_.reset();
    if (js_) { quickjs::JS_FreeContext(js_); js_ = nullptr; }
    if (rt_) { quickjs::JS_FreeRuntime(rt_); rt_ = nullptr; }
    url_.clear();
    rawHtml_.clear();
    loading_ = true;
}

js::VM* Tab::vm() const { return vm_.get(); }

std::string Tab::allText() const {
    return document_ ? document_->gatherText() : std::string();
}

html::DOMNode* Tab::getElementById(const std::string& id) {
    return document_ ? document_->getElementById(id) : nullptr;
}

std::vector<html::DOMNode*> Tab::getElementsByTagName(const std::string& tag) {
    if (!document_) return {};
    return document_->getElementsByTagName(tag);
}

html::DOMNode* Tab::querySelector(const std::string& selector) {
    return document_ ? document_->querySelector(selector) : nullptr;
}

std::vector<html::DOMNode*> Tab::querySelectorAll(const std::string& selector) {
    if (!document_) return {};
    return document_->querySelectorAll(selector);
}

// ── JS engine integration (Bead B) ───────────────────────────────────────

void Tab::initJS() {
    if (rt_ && js_) return;
    rt_ = quickjs::JS_NewRuntime();
    js_ = quickjs::JS_NewContext(rt_);
    quickjs::JS_SetOpaque(js_, this);
    bindDOM();
}

void Tab::bindDOM() {
    if (!js_) return;

    // document.getElementById(id) → wrapped element
    auto* docObj = quickjs::JS_NewObject("Document");
    docObj->native = document_.get();

    docObj->getter = [this](const std::string& key) -> quickjs::JSValue {
        if (key == "body" && document_) {
            html::DOMNode* body = nullptr;
            for (html::DOMNode* c = document_->firstChild; c; c = c->nextSibling) {
                if (c->nodeType == html::NodeType::Element && c->tagName == "html") {
                    for (html::DOMNode* h = c->firstChild; h; h = h->nextSibling) {
                        if (h->nodeType == html::NodeType::Element && h->tagName == "body") {
                            body = h; break;
                        }
                    }
                    break;
                }
            }
            if (body) return quickjs::JSValue::object(wrapNode(body));
        }
        if (key == "documentElement" && document_) {
            for (html::DOMNode* c = document_->firstChild; c; c = c->nextSibling) {
                if (c->nodeType == html::NodeType::Element && c->tagName == "html")
                    return quickjs::JSValue::object(wrapNode(c));
            }
        }
        return quickjs::JSValue::undefined();
    };

    // document.getElementById
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!document_ || args.empty()) return quickjs::JSValue::null();
            std::string id = args[0].type == quickjs::JSValueType::String ? args[0].str
                                                                         : quickjs::jsToString(args[0]);
            html::DOMNode* node = document_->getElementById(id);
            return node ? quickjs::JSValue::object(wrapNode(node))
                        : quickjs::JSValue::null();
        };
        docObj->props["getElementById"] = quickjs::JSValue::object(fn);
    }
    // document.createElement
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            html::DOMNode* node = new html::DOMNode();
            node->nodeType = html::NodeType::Element;
            node->tagName = args.empty() ? "" : (args[0].type == quickjs::JSValueType::String
                                                    ? args[0].str : quickjs::jsToString(args[0]));
            node->ownerDocument = document_.get();
            return quickjs::JSValue::object(wrapNode(node));
        };
        docObj->props["createElement"] = quickjs::JSValue::object(fn);
    }
    // document.querySelector
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!document_ || args.empty()) return quickjs::JSValue::null();
            std::string sel = args[0].type == quickjs::JSValueType::String ? args[0].str
                                                                          : quickjs::jsToString(args[0]);
            html::DOMNode* node = document_->querySelector(sel);
            return node ? quickjs::JSValue::object(wrapNode(node))
                        : quickjs::JSValue::null();
        };
        docObj->props["querySelector"] = quickjs::JSValue::object(fn);
    }

    quickjs::JS_SetGlobal(js_, "document", quickjs::JSValue::object(docObj));

    // requestAnimationFrame(cb)
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (args.empty()) return quickjs::JSValue::undefined();
            return quickjs::JSValue::number(
                quickjs::JS_RequestAnimationFrame(js_, args[0]));
        };
        quickjs::JS_SetGlobal(js_, "requestAnimationFrame", quickjs::JSValue::object(fn));
    }
    // setTimeout(cb, delay)
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (args.empty()) return quickjs::JSValue::undefined();
            double delay = args.size() > 1 ? quickjs::jsToNumber(args[1]) : 0.0;
            return quickjs::JSValue::number(quickjs::JS_SetTimeout(js_, args[0], delay));
        };
        quickjs::JS_SetGlobal(js_, "setTimeout", quickjs::JSValue::object(fn));
    }
    // setInterval(cb, delay)
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (args.empty()) return quickjs::JSValue::undefined();
            double delay = args.size() > 1 ? quickjs::jsToNumber(args[1]) : 16.0;
            return quickjs::JSValue::number(quickjs::JS_SetInterval(js_, args[0], delay));
        };
        quickjs::JS_SetGlobal(js_, "setInterval", quickjs::JSValue::object(fn));
    }
    // clearTimeout / clearInterval
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!args.empty()) quickjs::JS_ClearTimeout(js_, quickjs::jsToNumber(args[0]));
            return quickjs::JSValue::undefined();
        };
        quickjs::JS_SetGlobal(js_, "clearTimeout", quickjs::JSValue::object(fn));
        auto* fn2 = quickjs::JS_NewObject("Function");
        fn2->call = [this](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!args.empty()) quickjs::JS_ClearInterval(js_, quickjs::jsToNumber(args[0]));
            return quickjs::JSValue::undefined();
        };
        quickjs::JS_SetGlobal(js_, "clearInterval", quickjs::JSValue::object(fn2));
    }
    // console.log
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            std::string out;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += " ";
                out += quickjs::jsToString(args[i]);
            }
            util::Log(util::LogLevel::Info, "[JS] " + out + "\n");
            return quickjs::JSValue::undefined();
        };
        auto* console = quickjs::JS_NewObject("Object");
        console->props["log"] = quickjs::JSValue::object(fn);
        quickjs::JS_SetGlobal(js_, "console", quickjs::JSValue::object(console));
    }
}

quickjs::JSObject* Tab::wrapNode(html::DOMNode* node) {
    auto* obj = quickjs::JS_NewObject(node ? node->tagName : "Node");
    obj->native = node;

    // .innerHTML getter/setter
    obj->getter = [node](const std::string& key) -> quickjs::JSValue {
        if (!node) return quickjs::JSValue::undefined();
        if (key == "innerHTML") {
            std::string out;
            for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
                out += html::serializeNode(c);
            }
            return quickjs::JSValue::string(out);
        }
        if (key == "textContent") {
            std::string out;
            for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling) {
                out += c->gatherText();
            }
            return quickjs::JSValue::string(out);
        }
        if (key == "tagName") return quickjs::JSValue::string(node->tagName);
        if (key == "id") {
            auto* v = node->getAttribute("id");
            return v ? quickjs::JSValue::string(*v) : quickjs::JSValue::string("");
        }
        if (key == "children" || key == "childNodes") {
            auto* arr = quickjs::JS_NewObject("Array");
            size_t idx = 0;
            for (html::DOMNode* c = node->firstChild; c; c = c->nextSibling, ++idx) {
                arr->props[std::to_string(idx)] = quickjs::JSValue::object(wrapNode(c));
            }
            arr->props["length"] = quickjs::JSValue::number(static_cast<double>(idx));
            return quickjs::JSValue::object(arr);
        }
        if (key == "style") {
            auto* style = quickjs::JS_NewObject("CSSStyleDeclaration");
            style->setter = [node](const std::string& p, const quickjs::JSValue& v) {
                if (!node) return;
                node->attributes["style"] = (node->attributes.count("style")
                    ? node->attributes["style"] + ";" : "")
                    + p + ":" + quickjs::jsToString(v);
            };
            return quickjs::JSValue::object(style);
        }
        if (key == "appendChild" || key == "setAttribute" || key == "getElementById") {
            // handled via props below
        }
        // attribute access (getAttribute)
        auto* v = node->getAttribute(key);
        if (v) return quickjs::JSValue::string(*v);
        return quickjs::JSValue::undefined();
    };
    obj->setter = [node](const std::string& key, const quickjs::JSValue& v) {
        if (!node) return;
        if (key == "innerHTML") {
            node->setInnerHTML(quickjs::jsToString(v));
        } else if (key == "textContent") {
            node->setInnerHTML("");
            html::DOMNode* t = new html::DOMNode();
            t->nodeType = html::NodeType::Text;
            t->textContent = quickjs::jsToString(v);
            node->appendChild(t);
        } else if (key == "id") {
            node->setAttribute("id", quickjs::jsToString(v));
        } else {
            node->setAttribute(key, quickjs::jsToString(v));
        }
    };

    // .appendChild(child)
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [node](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!node || args.empty() || args[0].type != quickjs::JSValueType::Object
                || !args[0].obj || !args[0].obj->native)
                return quickjs::JSValue::undefined();
            html::DOMNode* child = static_cast<html::DOMNode*>(args[0].obj->native);
            if (child->ownerDocument == nullptr && node->ownerDocument)
                child->ownerDocument = node->ownerDocument;
            node->appendChild(child);
            return args[0];
        };
        obj->props["appendChild"] = quickjs::JSValue::object(fn);
    }
    // .setAttribute(name, value)
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [node](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!node || args.size() < 2) return quickjs::JSValue::undefined();
            node->setAttribute(quickjs::jsToString(args[0]), quickjs::jsToString(args[1]));
            return quickjs::JSValue::undefined();
        };
        obj->props["setAttribute"] = quickjs::JSValue::object(fn);
    }
    // .getElementById
    {
        auto* fn = quickjs::JS_NewObject("Function");
        fn->call = [node](const std::vector<quickjs::JSValue>& args) -> quickjs::JSValue {
            if (!node || args.empty()) return quickjs::JSValue::null();
            std::string id = args[0].type == quickjs::JSValueType::String ? args[0].str
                                                                         : quickjs::jsToString(args[0]);
            html::DOMNode* found = node->getElementById(id);
            return found ? quickjs::JSValue::object(wrapNode(found))
                         : quickjs::JSValue::null();
        };
        obj->props["getElementById"] = quickjs::JSValue::object(fn);
    }
    return obj;
}

void Tab::evalScript(const std::string& code) {
    if (!js_ || code.empty()) return;
    quickjs::JS_Eval(js_, code, "inline");
    if (js_->exception) {
        util::Log(util::LogLevel::Error, "[JS] " + js_->error + "\n");
        js_->exception = false;
    }
}

void Tab::runScripts() {
    if (!js_ || !document_) return;
    // Re-bind document to the freshly parsed tree.
    bindDOM();
    // Walk the tree for <script> elements and execute their text content.
    std::vector<html::DOMNode*> scripts = document_->getElementsByTagName("script");
    for (html::DOMNode* script : scripts) {
        std::string code;
        for (html::DOMNode* c = script->firstChild; c; c = c->nextSibling) {
            if (c->nodeType == html::NodeType::Text) code += c->textContent;
        }
        if (!code.empty()) evalScript(code);
    }
}

void Tab::runJSEventLoop() {
    if (!js_ || !rt_) return;
    quickjs::JS_ExecutePendingJobs(js_);
    quickjs::JS_DispatchAnimationFrame(js_, 0.0);
}

// ── private helpers ──────────────────────────────────────────────────────

void Tab::parseHTML(const std::string& html) {
    html::HTMLParser parser;
    document_ = std::unique_ptr<html::Document>(parser.parse(html));
}

void Tab::cascadeStyles() {
    if (!document_) return;

    document_->stylesheets.clear();

    css::CSSParser parser;
    std::vector<html::DOMNode*> styleElements = document_->getElementsByTagName("style");
    for (html::DOMNode* styleEl : styleElements) {
        std::string cssText;
        for (html::DOMNode* c = styleEl->firstChild; c; c = c->nextSibling) {
            if (c->nodeType == html::NodeType::Text) {
                cssText += c->textContent;
            }
        }
        if (!cssText.empty()) {
            auto rules = parser.parse(cssText);
            document_->stylesheets.insert(
                document_->stylesheets.end(), rules.begin(), rules.end());
        }
    }

    for (html::DOMNode* c = document_->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Element && c->tagName == "html") {
            for (html::DOMNode* h = c->firstChild; h; h = h->nextSibling) {
                if (h->nodeType == html::NodeType::Element && h->tagName == "head") {
                    for (html::DOMNode* s = h->firstChild; s; s = s->nextSibling) {
                        if (s->nodeType == html::NodeType::Element && s->tagName == "style") {
                            std::string cssText;
                            for (html::DOMNode* t = s->firstChild; t; t = t->nextSibling) {
                                if (t->nodeType == html::NodeType::Text) {
                                    cssText += t->textContent;
                                }
                            }
                            if (!cssText.empty()) {
                                auto rules = parser.parse(cssText);
                                document_->stylesheets.insert(
                                    document_->stylesheets.end(), rules.begin(), rules.end());
                            }
                        }
                    }
                }
            }
        }
    }

    for (html::DOMNode* c = document_->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Element) {
            css::ComputedStyle style = css::Cascade::computeStyle(c, document_.get());
            c->style = new css::ComputedStyle(style);
            for (html::DOMNode* desc = c->firstChild; desc; desc = desc->nextSibling) {
                if (desc->nodeType == html::NodeType::Element) {
                    css::ComputedStyle dstyle = css::Cascade::computeStyle(desc, document_.get());
                    desc->style = new css::ComputedStyle(dstyle);
                }
            }
        }
    }
}

void Tab::performLayout() {
    if (!document_) return;
    layout::Box engine;
    float vw = static_cast<float>(webView_.width);
    float vh = static_cast<float>(webView_.height);
    float sy = static_cast<float>(webView_.scrollY);
    layoutTree_ = std::make_unique<std::vector<std::unique_ptr<layout::LayoutNode>>>(engine.layout(document_.get(), vw, vh, sy));
}

void Tab::paintFrame() {
    if (!document_) {
        bodyText_ = "(no content loaded)";
        return;
    }
    html::DOMNode* bodyNode = nullptr;
    for (html::DOMNode* child = document_->firstChild; child; child = child->nextSibling) {
        if (child->nodeType == html::NodeType::Element && child->tagName == "body") {
            bodyNode = child;
            break;
        }
    }
    bodyText_ = extractTextContent(bodyNode);
    paintUs_ = 0.0;
}

} // namespace browser
