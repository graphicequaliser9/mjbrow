/**
 * @file tests/JSTest.cpp
 * @brief Integration tests for the JS runtime (Bead B).
 * @details Loads a page containing an inline <script> that mutates the DOM via
 *          document.body.innerHTML, then asserts the rendered tree updated.
 *          Also exercises requestAnimationFrame / setTimeout directly through
 *          the quickjs engine API.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "js/QuickJS.h"
#include "browser/Tab.h"
#include "html/DOMNode.h"
#include "html/HTMLParser.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace quickjs;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "FAIL: " #cond "  (" << __FILE__ << ":" << __LINE__ \
                      << ")\n";                                             \
        }                                                                    \
    } while (0)

// ── engine smoke tests ───────────────────────────────────────────────────────

static void testEngineArithmetic() {
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    JSValue v = JS_Eval(ctx, "1 + 2 * 3", "t.js");
    CHECK(!ctx->exception);
    CHECK(v.type == JSValueType::Number);
    CHECK(v.num == 7.0);

    JSValue s = JS_Eval(ctx, "'Hello' + ' ' + 'World'", "t.js");
    CHECK(!ctx->exception);
    CHECK(s.type == JSValueType::String);
    CHECK(s.str == "Hello World");

    JS_ExecutePendingJobs(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void testEngineExceptions() {
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    // Calling a non-function must set the exception flag without crashing.
    JS_Eval(ctx, "var x = 5; x();", "t.js");
    CHECK(ctx->exception);
    std::string err = JS_GetException(ctx);
    CHECK(!err.empty());
    // Clearing the flag lets subsequent scripts run fine.
    ctx->exception = false;

    JSValue ok = JS_Eval(ctx, "2 + 2", "t.js");
    CHECK(!ctx->exception);
    CHECK(ok.num == 4.0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void testRafAndTimers() {
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    bool rafFired = false;
    bool timeoutFired = false;

    // Build native callbacks and schedule them via the API.
    auto* rafFn = JS_NewObject("Function");
    rafFn->call = [&](const std::vector<JSValue>&) -> JSValue {
        rafFired = true;
        return JSValue::undefined();
    };
    JS_RequestAnimationFrame(ctx, JSValue::object(rafFn));

    auto* toFn = JS_NewObject("Function");
    toFn->call = [&](const std::vector<JSValue>&) -> JSValue {
        timeoutFired = true;
        return JSValue::undefined();
    };
    JS_SetTimeout(ctx, JSValue::object(toFn), 0.0);

    // Drive several frames: each frame runs pending jobs + due timers + RAF.
    for (int i = 0; i < 3; ++i) {
        JS_ExecutePendingJobs(ctx);
        JS_DispatchAnimationFrame(ctx, static_cast<double>(i) * 16.0);
    }

    CHECK(rafFired);
    CHECK(timeoutFired);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// ── DOM binding integration (the required acceptance test) ────────────────────

static const html::DOMNode* findElement(const html::DOMNode* n, const std::string& tag) {
    if (!n) return nullptr;
    if (n->nodeType == html::NodeType::Element && n->tagName == tag) return n;
    for (const html::DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (const html::DOMNode* found = findElement(c, tag)) return found;
    }
    return nullptr;
}

static std::string textContentOf(const html::DOMNode* n) {
    std::string out;
    if (!n) return out;
    for (const html::DOMNode* c = n->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Text) out += c->textContent;
        else if (c->nodeType == html::NodeType::Element) out += textContentOf(c);
    }
    return out;
}

static void testScriptUpdatesDOM() {
    browser::Tab tab;
    // The acceptance test from the bead: a script that appends to body.innerHTML.
    tab.loadHTML(
        "<!DOCTYPE html><html><head></head>"
        "<body><p>Before</p>"
        "<script>document.body.innerHTML += '<p>Hello</p>'</script>"
        "</body></html>");

    const html::DOMNode* doc = tab.document();
    CHECK(doc != nullptr);

    const html::DOMNode* body = nullptr;
    for (const html::DOMNode* c = doc->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Element && c->tagName == "html") {
            for (const html::DOMNode* h = c->firstChild; h; h = h->nextSibling) {
                if (h->nodeType == html::NodeType::Element && h->tagName == "body") {
                    body = h; break;
                }
            }
            break;
        }
    }
    CHECK(body != nullptr);

    // The script must have appended a <p>Hello</p> inside <body>.
    const html::DOMNode* helloP = findElement(body, "p");
    CHECK(helloP != nullptr);
    std::string text = textContentOf(body);
    CHECK(text.find("Hello") != std::string::npos);
    CHECK(text.find("Before") != std::string::npos);

    // The body must now contain the injected <p> (two <p> children).
    int pCount = 0;
    for (const html::DOMNode* c = body->firstChild; c; c = c->nextSibling) {
        if (c->nodeType == html::NodeType::Element && c->tagName == "p") ++pCount;
    }
    CHECK(pCount == 2);
}

static void testGetElementByIdBinding() {
    browser::Tab tab;
    tab.loadHTML(
        "<html><body>"
        "<div id='box'>initial</div>"
        "<script>"
        "  var el = document.getElementById('box');"
        "  el.innerHTML = 'changed';"
        "</script>"
        "</body></html>");

    const html::DOMNode* box = tab.getElementById("box");
    CHECK(box != nullptr);
    CHECK(textContentOf(box) == "changed");
}

static void testStyleBinding() {
    browser::Tab tab;
    tab.loadHTML(
        "<html><body>"
        "<div id='d'>x</div>"
        "<script>"
        "  document.getElementById('d').style.color = 'red';"
        "</script>"
        "</body></html>");

    const html::DOMNode* d = tab.getElementById("d");
    CHECK(d != nullptr);
    auto* style = d->getAttribute("style");
    CHECK(style != nullptr);
    CHECK(style->find("color:red") != std::string::npos);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    testEngineArithmetic();
    testEngineExceptions();
    testRafAndTimers();
    testScriptUpdatesDOM();
    testGetElementByIdBinding();
    testStyleBinding();

    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
