/**
 * @file js/QuickJSContext.cpp
 * @brief QuickJS JSContext wrapper and DOM host-object bindings.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "js/QuickJSContext.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "quickjs.h"
}

namespace js {

namespace {

/// @brief Opaque JS class id for wrapped DOM nodes (set once at runtime).
JSClassID kDOMNodeClassId = 0;

/// @brief Finalizer for wrapped DOM nodes. The DOMNode is owned by the document,
///        so we only release the JS-side wrapper, never delete the C++ node.
void domNodeFinalizer(JSRuntime* /*rt*/, JSValue val) {
    void* p = JS_GetOpaque(val, kDOMNodeClassId);
    (void)p; // node lifetime is owned by the document, not the JS wrapper
}

JSClassDef kDOMNodeClass = {
    "DOMNode",
    .finalizer = domNodeFinalizer,
};

} // namespace

QuickJSContext::QuickJSContext() {
    rt_ = JS_NewRuntime();
    ctx_ = JS_NewContext(rt_);

    if (kDOMNodeClassId == 0) {
        JS_NewClassID(rt_, &kDOMNodeClassId);
    }
    JS_NewClass(rt_, kDOMNodeClassId, &kDOMNodeClass);

    JS_SetContextOpaque(ctx_, this);

    setupWindow();
    setupDocument();
}

QuickJSContext::~QuickJSContext() {
    if (ctx_) {
        JS_RunGC(rt_);
        JS_FreeContext(ctx_);
    }
    if (rt_) JS_FreeRuntime(rt_);
}

std::string QuickJSContext::evaluate(const std::string& code) {
    JSValue result = JS_Eval(ctx_, code.c_str(), code.size(), "<script>",
                             JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        has_exception_ = true;
        JSValue exc = JS_GetException(ctx_);
        const char* msg = JS_ToCString(ctx_, exc);
        last_error_ = msg ? msg : "unknown exception";
        if (msg) JS_FreeCString(ctx_, msg);
        JS_FreeValue(ctx_, exc);
        JS_FreeValue(ctx_, result);
        return last_error_;
    }

    const char* str = JS_ToCString(ctx_, result);
    std::string out = str ? str : "";
    if (str) JS_FreeCString(ctx_, str);
    JS_FreeValue(ctx_, result);
    last_error_.clear();
    has_exception_ = false;
    return out;
}

int QuickJSContext::executePendingJobs() {
    // Reset any stale exception flag so a prior error does not persist across
    // calls (fix #5: has_exception_ must be cleared at the start).
    has_exception_ = false;

    int executed = 0;
    int rc;
    while ((rc = JS_ExecutePendingJob(rt_, &ctx_)) > 0) {
        ++executed;
    }
    if (rc < 0) {
        has_exception_ = true;
        JSValue exc = JS_GetException(ctx_);
        const char* msg = JS_ToCString(ctx_, exc);
        last_error_ = msg ? msg : "exception in pending job";
        if (msg) JS_FreeCString(ctx_, msg);
        JS_FreeValue(ctx_, exc);
        return -1;
    }
    return executed;
}

void QuickJSContext::clearException() {
    has_exception_ = false;
    last_error_.clear();
}

void QuickJSContext::setupWindow() {
    JSValue global = JS_GetGlobalObject(ctx_);

    // Create the persistent `document` host object. The underlying C++ document
    // pointer is read lazily at call time (via the context opaque), so the JS
    // object can be installed once at construction regardless of document state.
    JSValue documentVal = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, global, "document", documentVal);

    JS_FreeValue(ctx_, global);
    // `documentVal` is now owned by the global object; no leak (fix #4: we store
    // the original directly rather than dup-and-leak the prior reference).
}

void QuickJSContext::setupDocument() {
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue documentVal = JS_GetPropertyStr(ctx_, global, "document");

    JS_SetPropertyStr(ctx_, documentVal, "getElementById",
                      JS_NewCFunction(ctx_, &QuickJSContext::documentGetElementById,
                                      "getElementById", 1));
    JS_SetPropertyStr(ctx_, documentVal, "querySelector",
                      JS_NewCFunction(ctx_, &QuickJSContext::documentQuerySelector,
                                      "querySelector", 1));
    JS_SetPropertyStr(ctx_, documentVal, "querySelectorAll",
                      JS_NewCFunction(ctx_, &QuickJSContext::documentQuerySelectorAll,
                                      "querySelectorAll", 1));

    JS_FreeValue(ctx_, documentVal);
    JS_FreeValue(ctx_, global);
}

JSValue QuickJSContext::wrapNode(html::DOMNode* node) {
    if (!node) return JS_NULL;
    JSValue obj = JS_NewObjectClass(ctx_, kDOMNodeClassId);
    if (JS_IsException(obj)) return JS_NULL;
    JS_SetOpaque(obj, node);
    return obj;
}

JSValue QuickJSContext::documentGetElementById(JSContext* ctx, JSValueConst /*this_val*/,
                                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    const char* id = JS_ToCString(ctx, argv[0]);
    // Do NOT free `id` yet: it is still needed for the DOM lookup below.
    // (fix #1: JS_FreeCString moved after getElementById)

    auto* self = static_cast<QuickJSContext*>(JS_GetContextOpaque(ctx));
    html::DOMNode* found = self && self->document_
                               ? self->document_->getElementById(id)
                               : nullptr;

    JS_FreeCString(ctx, id);  // safe now — lookup is complete

    return self->wrapNode(found);
}

JSValue QuickJSContext::documentQuerySelector(JSContext* ctx, JSValueConst /*this_val*/,
                                              int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    const char* selector = JS_ToCString(ctx, argv[0]);
    // (fix #2: free the cstring only after the lookup)

    auto* self = static_cast<QuickJSContext*>(JS_GetContextOpaque(ctx));
    html::DOMNode* found = self && self->document_
                               ? self->document_->querySelector(selector)
                               : nullptr;

    JS_FreeCString(ctx, selector);  // safe now — lookup is complete

    return self->wrapNode(found);
}

JSValue QuickJSContext::documentQuerySelectorAll(JSContext* ctx, JSValueConst /*this_val*/,
                                                 int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    const char* selector = JS_ToCString(ctx, argv[0]);
    // (fix #3: free the cstring only after the lookup)

    auto* self = static_cast<QuickJSContext*>(JS_GetContextOpaque(ctx));
    std::vector<html::DOMNode*> found = self && self->document_
                                            ? self->document_->querySelectorAll(selector)
                                            : std::vector<html::DOMNode*>{};

    JS_FreeCString(ctx, selector);  // safe now — lookup is complete

    JSValue array = JS_NewArray(ctx);
    if (JS_IsException(array)) return JS_NULL;
    for (size_t i = 0; i < found.size(); ++i) {
        JSValue node = self->wrapNode(found[i]);
        JS_SetPropertyUint32(ctx, array, static_cast<uint32_t>(i), node);
    }
    return array;
}

} // namespace js
