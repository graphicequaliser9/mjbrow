/**
 * @file js/QuickJS.h
 * @brief Minimal QuickJS-compatible JavaScript engine embedding layer.
 * @details Provides JSRuntime, JSContext, and JSValue types with a subset
 *          of the QuickJS C API sufficient for browser integration.
 *          The engine supports ES2020 basics: expressions, statements,
 *          functions, member access, and DOM bindings.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef JS_QUICKJS_H
#define JS_QUICKJS_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace quickjs {

struct JSValue;
struct JSObject;
struct JSRuntime;
struct JSContext;

// ── JSValue ──────────────────────────────────────────────────────────────────

enum class JSValueType {
    Undefined,
    Null,
    Bool,
    Number,
    String,
    Object,
    Function
};

struct JSValue {
    JSValueType type;
    union {
        bool b;
        double num;
    };
    std::string str;
    JSObject* obj;

    JSValue() : type(JSValueType::Undefined), num(0.0), obj(nullptr) {}
    static JSValue undefined();
    static JSValue null();
    static JSValue boolVal(bool b);
    static JSValue number(double n);
    static JSValue string(const std::string& s);
    static JSValue object(JSObject* o);
};

// ── JSObject ─────────────────────────────────────────────────────────────────

struct JSObject {
    std::map<std::string, JSValue> props;
    JSObject* prototype;
    void* native;
    std::string typeName;

    std::function<JSValue(const std::string&)> getter;
    std::function<void(const std::string&, const JSValue&)> setter;
    std::function<JSValue(const std::vector<JSValue>&)> call;

    JSObject();
    ~JSObject();
};

// ── Timer / RAF types ─────────────────────────────────────────────────────────

using JSTimerId = double;

// ── JSRuntime ────────────────────────────────────────────────────────────────

struct JSRuntime {
    std::vector<std::function<void(JSContext*)>> pendingJobs;
    std::vector<std::pair<std::function<void()>, JSTimerId>> timers;
    JSTimerId nextTimerId{1.0};
    JSTimerId nextRafId{1.0};
    ~JSRuntime();
};

// ── JSContext ────────────────────────────────────────────────────────────────

struct JSContext {
    JSObject global;
    JSRuntime* rt;
    void* opaque;
    std::string error;
    bool exception;

    JSContext(JSRuntime* runtime);
    ~JSContext();
};

// ── C-compatible API ─────────────────────────────────────────────────────────

JSRuntime* JS_NewRuntime();
void JS_FreeRuntime(JSRuntime* rt);

JSContext* JS_NewContext(JSRuntime* rt);
void JS_FreeContext(JSContext* ctx);

JSValue JS_Eval(JSContext* ctx, const std::string& code, const std::string& filename);
void JS_ExecutePendingJobs(JSContext* ctx);

void JS_SetGlobal(JSContext* ctx, const std::string& name, JSValue val);
JSValue JS_GetGlobal(JSContext* ctx, const std::string& name);

JSTimerId JS_SetTimeout(JSContext* ctx, JSValue func, double delay);
JSTimerId JS_SetInterval(JSContext* ctx, JSValue func, double delay);
void JS_ClearTimeout(JSContext* ctx, JSTimerId id);
void JS_ClearInterval(JSContext* ctx, JSTimerId id);

JSTimerId JS_RequestAnimationFrame(JSContext* ctx, JSValue func);

void JS_SetOpaque(JSContext* ctx, void* opaque);
void* JS_GetOpaque(JSContext* ctx);

bool JS_IsException(JSValue val);
std::string JS_GetException(JSContext* ctx);

/// @brief Converts any JSValue to its JS string representation.
std::string jsToString(const JSValue& v);

/// @brief Converts any JSValue to a double (NaN on failure).
double jsToNumber(const JSValue& v);

/// @brief Converts any JSValue to a boolean.
bool jsToBool(const JSValue& v);

/**
 * @brief Fires every requestAnimationFrame callback currently queued, passing a
 *        high-resolution timestamp.  Called once per frame from the browser's
 *        message loop after JS_ExecutePendingJobs().
 */
void JS_DispatchAnimationFrame(JSContext* ctx, double timestampMs);

/**
 * @brief Allocates a fresh JSObject with the given type name.
 *        Used by the DOM binding layer to wrap native DOM nodes.
 */
JSObject* JS_NewObject(const std::string& typeName = "Object");

} // namespace quickjs

#endif // JS_QUICKJS_H
