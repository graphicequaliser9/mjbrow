/**
 * @file js/QuickJSContext.h
 * @brief Thin C++ wrapper around a QuickJS JSContext plus DOM host objects.
 * @details Owns a JSRuntime + JSContext, exposes script evaluation, microtask
 *          draining and the `window` / `document` host objects used by the
 *          browser runtime.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef JS_QUICKJSCONTEXT_H
#define JS_QUICKJSCONTEXT_H

#include <string>
#include <vector>

#include "html/DOMNode.h"

extern "C" {
#include "quickjs.h"
}

namespace js {

/**
 * @class QuickJSContext
 * @brief Wraps a single QuickJS runtime/context and binds the DOM host objects.
 */
class QuickJSContext {
public:
    /// @brief Constructs and initialises an isolated JS context.
    QuickJSContext();
    ~QuickJSContext();

    QuickJSContext(const QuickJSContext&) = delete;
    QuickJSContext& operator=(const QuickJSContext&) = delete;

    /// @brief Sets the document exposed to scripts (the `document` host object).
    void setDocument(html::DOMNode* doc) { document_ = doc; }

    /// @brief Evaluates @p code in the global scope.
    /// @return The string form of the result, or an error description.
    std::string evaluate(const std::string& code);

    /// @brief Drains the pending-job (microtask) queue.
    /// @return The number of jobs executed, or a negative value on error.
    int executePendingJobs();

    /// @brief True if the last evaluation raised (and kept) an exception.
    bool hasException() const { return has_exception_; }

    /// @brief Returns the last exception message (empty when none).
    const std::string& lastError() const { return last_error_; }

    /// @brief Clears any pending exception state and message.
    void clearException();

private:
    /// @brief Installs `window` and its `document` property on the global object.
    void setupWindow();

    /// @brief Binds `document.getElementById/querySelector/querySelectorAll`.
    void setupDocument();

    // ── host-object native methods (JS_CFUNC) ──────────────────────────────────
    static JSValue documentGetElementById(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv);
    static JSValue documentQuerySelector(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv);
    static JSValue documentQuerySelectorAll(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv);

    /// @brief Wraps an html::DOMNode* as a JS object, or JS_NULL when null.
    JSValue wrapNode(html::DOMNode* node);

    JSRuntime* rt_{nullptr};
    JSContext* ctx_{nullptr};
    html::DOMNode* document_{nullptr};
    bool has_exception_{false};
    std::string last_error_;
};

} // namespace js

#endif // JS_QUICKJSCONTEXT_H
