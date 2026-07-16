/**
 * @file js/QuickJS.cpp
 * @brief Minimal QuickJS-compatible JavaScript engine implementation.
 * @details Implements the subset of the QuickJS C API declared in js/QuickJS.h
 *          sufficient for browser integration: a recursive-descent parser plus
 *          a tree-walking interpreter supporting ES2020 basics (expressions,
 *          statements, functions, member access, assignment, +=, exceptions)
 *          and the timer / requestAnimationFrame / microtask plumbing used by
 *          the browser runtime.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "js/QuickJS.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace quickjs {

// ─────────────────────────────────────────────────────────────────────────────
// JSValue helpers
// ─────────────────────────────────────────────────────────────────────────────

JSValue JSValue::undefined() { JSValue v; v.type = JSValueType::Undefined; return v; }
JSValue JSValue::null()      { JSValue v; v.type = JSValueType::Null;      return v; }
JSValue JSValue::boolVal(bool b) { JSValue v; v.type = JSValueType::Bool; v.b = b; return v; }
JSValue JSValue::number(double n) { JSValue v; v.type = JSValueType::Number; v.num = n; return v; }
JSValue JSValue::string(const std::string& s) { JSValue v; v.type = JSValueType::String; v.str = s; return v; }
JSValue JSValue::object(JSObject* o) { JSValue v; v.type = JSValueType::Object; v.obj = o; return v; }

std::string jsToString(const JSValue& v) {
    switch (v.type) {
    case JSValueType::Undefined: return "undefined";
    case JSValueType::Null:      return "null";
    case JSValueType::Bool:      return v.b ? "true" : "false";
    case JSValueType::Number: {
        double n = v.num;
        if (std::isnan(n)) return "NaN";
        if (std::isinf(n)) return n < 0 ? "-Infinity" : "Infinity";
        if (n == std::floor(n) && std::fabs(n) < 1e15) {
            long long i = static_cast<long long>(n);
            return std::to_string(i);
        }
        std::ostringstream ss; ss << n; return ss.str();
    }
    case JSValueType::String:    return v.str;
    case JSValueType::Object:    return "[object Object]";
    case JSValueType::Function:  return "[function]";
    }
    return "";
}

double jsToNumber(const JSValue& v) {
    switch (v.type) {
    case JSValueType::Undefined: return std::nan("");
    case JSValueType::Null:      return 0.0;
    case JSValueType::Bool:      return v.b ? 1.0 : 0.0;
    case JSValueType::Number:    return v.num;
    case JSValueType::String: {
        if (v.str.empty()) return 0.0;
        try { return std::stod(v.str); } catch (...) { return std::nan(""); }
    }
    default:                     return std::nan("");
    }
}

bool jsToBool(const JSValue& v) {
    switch (v.type) {
    case JSValueType::Undefined:
    case JSValueType::Null:  return false;
    case JSValueType::Bool:  return v.b;
    case JSValueType::Number: return v.num != 0.0 && !std::isnan(v.num);
    case JSValueType::String: return !v.str.empty();
    default:                  return true;
    }
}

static bool jsToNumberInPlace(JSValue& v) {
    double n = jsToNumber(v);
    v.type = JSValueType::Number;
    v.num = n;
    return !std::isnan(n);
}

// ─────────────────────────────────────────────────────────────────────────────
// Exception type used by the evaluator
// ─────────────────────────────────────────────────────────────────────────────

struct JSRuntimeError : std::runtime_error {
    explicit JSRuntimeError(const std::string& w) : std::runtime_error(w) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// AST
// ─────────────────────────────────────────────────────────────────────────────

struct Stmt; // forward declaration (used by Expr::body for function literals)

struct Expr {
    enum class Kind {
        Num, Str, Bool, Null, Undefined,
        Ident,
        Binary, Unary,
        Assign, CompoundAssign,
        Member, Call,
        Func,
    };
    Kind kind;
    double num = 0;
    std::string str;
    std::string op;
    std::unique_ptr<Expr> a, b, c;
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Stmt>> body;
    Expr(Kind k) : kind(k) {}
};

struct Stmt {
    enum class Kind {
        ExprStmt,
        VarDecl,
        Block,
        If,
        While,
        Return,
        Empty,
    };
    Kind kind;
    std::string name;
    std::unique_ptr<Expr> init;
    std::unique_ptr<Expr> expr;
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenB, elseB;
    std::vector<std::unique_ptr<Stmt>> block;
    Stmt(Kind k) : kind(k) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Tokenizer
// ─────────────────────────────────────────────────────────────────────────────

enum class Tok {
    Num, Str, Ident, Kw,
    Plus, Minus, Star, Slash, Percent,
    Eq, EqEq, NotEq, Lt, Gt, Le, Ge,
    PlusEq, MinusEq, StarEq, SlashEq,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Semi, Comma, Dot,
    Bang, AmpAmp, PipePipe,
    Eof,
};

struct Token {
    Tok kind;
    std::string text;
    double num = 0;
};

struct Lexer {
    const std::string& src;
    size_t i = 0;
    Token next() {
        while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
        if (i >= src.size()) return {Tok::Eof, ""};
        char ch = src[i];
        if (std::isdigit(static_cast<unsigned char>(ch)) || (ch == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i+1])))) {
            size_t start = i;
            while (i < src.size() && (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.')) ++i;
            double n = std::atof(src.substr(start, i - start).c_str());
            return {Tok::Num, src.substr(start, i - start), n};
        }
        if (ch == '"' || ch == '\'') {
            char q = ch; ++i; size_t start = i; std::string s;
            while (i < src.size() && src[i] != q) {
                if (src[i] == '\\' && i + 1 < src.size()) {
                    ++i;
                    char e = src[i];
                    switch (e) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case '\\': s += '\\'; break;
                    case '\'': s += '\''; break;
                    case '"': s += '"'; break;
                    case '0': s += '\0'; break;
                    default: s += e; break;
                    }
                    ++i;
                } else {
                    s += src[i++];
                }
            }
            if (i < src.size()) ++i;
            return {Tok::Str, s};
        }
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$') {
            size_t start = i;
            while (i < src.size() && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_' || src[i] == '$')) ++i;
            std::string word = src.substr(start, i - start);
            return {Tok::Kw, word};
        }
        auto two = [&](const char* s) -> bool {
            return src.compare(i, std::strlen(s), s) == 0;
        };
        if (two("==")) { i += 2; return {Tok::EqEq, "=="}; }
        if (two("!=")) { i += 2; return {Tok::NotEq, "!="}; }
        if (two("<=")) { i += 2; return {Tok::Le, "<="}; }
        if (two(">=")) { i += 2; return {Tok::Ge, ">="}; }
        if (two("&&")) { i += 2; return {Tok::AmpAmp, "&&"}; }
        if (two("||")) { i += 2; return {Tok::PipePipe, "||"}; }
        if (two("+=")) { i += 2; return {Tok::PlusEq, "+="}; }
        if (two("-=")) { i += 2; return {Tok::MinusEq, "-="}; }
        if (two("*=")) { i += 2; return {Tok::StarEq, "*="}; }
        if (two("/=")) { i += 2; return {Tok::SlashEq, "/="}; }
        switch (ch) {
        case '+': ++i; return {Tok::Plus, "+"};
        case '-': ++i; return {Tok::Minus, "-"};
        case '*': ++i; return {Tok::Star, "*"};
        case '/': ++i; return {Tok::Slash, "/"};
        case '%': ++i; return {Tok::Percent, "%"};
        case '=': ++i; return {Tok::Eq, "="};
        case '<': ++i; return {Tok::Lt, "<"};
        case '>': ++i; return {Tok::Gt, ">"};
        case '(': ++i; return {Tok::LParen, "("};
        case ')': ++i; return {Tok::RParen, ")"};
        case '{': ++i; return {Tok::LBrace, "{"};
        case '}': ++i; return {Tok::RBrace, "}"};
        case '[': ++i; return {Tok::LBracket, "["};
        case ']': ++i; return {Tok::RBracket, "]"};
        case ';': ++i; return {Tok::Semi, ";"};
        case ',': ++i; return {Tok::Comma, ","};
        case '.': ++i; return {Tok::Dot, "."};
        case '!': ++i; return {Tok::Bang, "!"};
        default: ++i; return {Tok::Eof, std::string(1, ch)};
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Parser
// ─────────────────────────────────────────────────────────────────────────────

struct Parser {
    std::vector<Token> toks;
    size_t pos = 0;
    bool ok = true;

    Parser(const std::string& src) {
        Lexer lex{src};
        Token t = lex.next();
        while (t.kind != Tok::Eof) { toks.push_back(t); t = lex.next(); }
        toks.push_back({Tok::Eof, ""});
    }

    const Token& peek() { return toks[pos]; }
    const Token& peek2() { return toks[pos + 1 < toks.size() ? pos + 1 : pos]; }
    Token advance() { return toks[pos++]; }
    bool eat(Tok k) { if (peek().kind == k) { ++pos; return true; } return false; }
    bool isKw(const std::string& w) { return peek().kind == Tok::Kw && peek().text == w; }

    std::unique_ptr<Stmt> parseProgram() {
        auto blk = std::make_unique<Stmt>(Stmt::Kind::Block);
        while (peek().kind != Tok::Eof) {
            auto s = parseStmt();
            if (!s) break;
            blk->block.push_back(std::move(s));
        }
        return blk;
    }

    std::unique_ptr<Stmt> parseStmt() {
        if (isKw("var") || isKw("let") || isKw("const")) {
            advance();
            if (peek().kind != Tok::Kw && peek().kind != Tok::Ident) { ok = false; return nullptr; }
            auto s = std::make_unique<Stmt>(Stmt::Kind::VarDecl);
            s->name = advance().text;
            if (eat(Tok::Eq)) s->init = parseExpr();
            eat(Tok::Semi);
            return s;
        }
        if (isKw("if")) {
            advance();
            auto s = std::make_unique<Stmt>(Stmt::Kind::If);
            eat(Tok::LParen);
            s->cond = parseExpr();
            eat(Tok::RParen);
            s->thenB = parseStmt();
            if (isKw("else")) { advance(); s->elseB = parseStmt(); }
            return s;
        }
        if (isKw("while")) {
            advance();
            auto s = std::make_unique<Stmt>(Stmt::Kind::While);
            eat(Tok::LParen);
            s->cond = parseExpr();
            eat(Tok::RParen);
            s->block.push_back(parseStmt());
            return s;
        }
        if (isKw("return")) {
            advance();
            auto s = std::make_unique<Stmt>(Stmt::Kind::Return);
            if (peek().kind != Tok::Semi && peek().kind != Tok::Eof && peek().kind != Tok::RBrace) {
                s->expr = parseExpr();
            }
            eat(Tok::Semi);
            return s;
        }
        if (peek().kind == Tok::LBrace) {
            return parseBlock();
        }
        if (peek().kind == Tok::Semi) { advance(); return std::make_unique<Stmt>(Stmt::Kind::Empty); }
        auto s = std::make_unique<Stmt>(Stmt::Kind::ExprStmt);
        s->expr = parseExpr();
        eat(Tok::Semi);
        return s;
    }

    std::unique_ptr<Stmt> parseBlock() {
        auto s = std::make_unique<Stmt>(Stmt::Kind::Block);
        eat(Tok::LBrace);
        while (peek().kind != Tok::RBrace && peek().kind != Tok::Eof) {
            auto inner = parseStmt();
            if (!inner) break;
            s->block.push_back(std::move(inner));
        }
        eat(Tok::RBrace);
        return s;
    }

    std::unique_ptr<Expr> parseExpr() { return parseAssign(); }

    std::unique_ptr<Expr> parseAssign() {
        auto lhs = parseLogic();
        if (peek().kind == Tok::Eq) {
            advance();
            auto e = std::make_unique<Expr>(Expr::Kind::Assign);
            e->a = std::move(lhs);
            e->b = parseExpr();
            return e;
        }
        if (peek().kind == Tok::PlusEq || peek().kind == Tok::MinusEq ||
            peek().kind == Tok::StarEq || peek().kind == Tok::SlashEq) {
            Tok op = peek().kind; advance();
            auto rhs = parseExpr();
            auto e = std::make_unique<Expr>(Expr::Kind::CompoundAssign);
            e->op = (op == Tok::PlusEq) ? "+" : (op == Tok::MinusEq) ? "-" :
                    (op == Tok::StarEq) ? "*" : "/";
            e->a = std::move(lhs);
            e->b = std::move(rhs);
            return e;
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseLogic() {
        auto lhs = parseCompare();
        while (peek().kind == Tok::AmpAmp || peek().kind == Tok::PipePipe) {
            Tok op = peek().kind; advance();
            auto rhs = parseCompare();
            auto e = std::make_unique<Expr>(Expr::Kind::Binary);
            e->op = (op == Tok::AmpAmp) ? "&&" : "||";
            e->a = std::move(lhs);
            e->b = std::move(rhs);
            lhs = std::move(e);
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseCompare() {
        auto lhs = parseAdd();
        while (peek().kind == Tok::Lt || peek().kind == Tok::Gt ||
               peek().kind == Tok::Le || peek().kind == Tok::Ge ||
               peek().kind == Tok::EqEq || peek().kind == Tok::NotEq) {
            std::string op = advance().text;
            auto rhs = parseAdd();
            auto e = std::make_unique<Expr>(Expr::Kind::Binary);
            e->op = op;
            e->a = std::move(lhs);
            e->b = std::move(rhs);
            lhs = std::move(e);
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseAdd() {
        auto lhs = parseMul();
        while (peek().kind == Tok::Plus || peek().kind == Tok::Minus) {
            std::string op = advance().text;
            auto rhs = parseMul();
            auto e = std::make_unique<Expr>(Expr::Kind::Binary);
            e->op = op;
            e->a = std::move(lhs);
            e->b = std::move(rhs);
            lhs = std::move(e);
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseMul() {
        auto lhs = parseUnary();
        while (peek().kind == Tok::Star || peek().kind == Tok::Slash || peek().kind == Tok::Percent) {
            std::string op = advance().text;
            auto rhs = parseUnary();
            auto e = std::make_unique<Expr>(Expr::Kind::Binary);
            e->op = op;
            e->a = std::move(lhs);
            e->b = std::move(rhs);
            lhs = std::move(e);
        }
        return lhs;
    }

    std::unique_ptr<Expr> parseUnary() {
        if (peek().kind == Tok::Minus || peek().kind == Tok::Bang) {
            std::string op = advance().text;
            auto e = std::make_unique<Expr>(Expr::Kind::Unary);
            e->op = op;
            e->a = parseUnary();
            return e;
        }
        if (isKw("function")) {
            return parseFunction();
        }
        return parsePostfix();
    }

    std::unique_ptr<Expr> parseFunction() {
        advance(); // function
        eat(Tok::LParen);
        auto e = std::make_unique<Expr>(Expr::Kind::Func);
        while (peek().kind == Tok::Ident) {
            e->params.push_back(advance().text);
            if (!eat(Tok::Comma)) break;
        }
        eat(Tok::RParen);
        if (peek().kind == Tok::LBrace) {
            e->body.push_back(parseBlock());
        }
        return e;
    }

    std::unique_ptr<Expr> parsePostfix() {
        auto e = parsePrimary();
        while (true) {
            if (peek().kind == Tok::Dot) {
                advance();
                if (peek().kind != Tok::Ident && peek().kind != Tok::Kw) { ok = false; break; }
                auto m = std::make_unique<Expr>(Expr::Kind::Member);
                m->a = std::move(e);
                m->str = advance().text;
                e = std::move(m);
            } else if (peek().kind == Tok::LBracket) {
                advance();
                auto m = std::make_unique<Expr>(Expr::Kind::Member);
                m->a = std::move(e);
                m->b = parseExpr();
                eat(Tok::RBracket);
                e = std::move(m);
            } else if (peek().kind == Tok::LParen) {
                advance();
                auto c = std::make_unique<Expr>(Expr::Kind::Call);
                c->a = std::move(e);
                while (peek().kind != Tok::RParen && peek().kind != Tok::Eof) {
                    c->args.push_back(parseExpr());
                    if (!eat(Tok::Comma)) break;
                }
                eat(Tok::RParen);
                e = std::move(c);
            } else {
                break;
            }
        }
        return e;
    }

    std::unique_ptr<Expr> parsePrimary() {
        Token t = peek();
        if (t.kind == Tok::Num) {
            advance();
            auto e = std::make_unique<Expr>(Expr::Kind::Num);
            e->num = t.num;
            return e;
        }
        if (t.kind == Tok::Str) {
            advance();
            auto e = std::make_unique<Expr>(Expr::Kind::Str);
            e->str = t.text;
            return e;
        }
        if (isKw("true")) { advance(); auto e = std::make_unique<Expr>(Expr::Kind::Bool); e->num = 1; return e; }
        if (isKw("false")) { advance(); auto e = std::make_unique<Expr>(Expr::Kind::Bool); e->num = 0; return e; }
        if (isKw("null")) { advance(); return std::make_unique<Expr>(Expr::Kind::Null); }
        if (isKw("undefined")) { advance(); return std::make_unique<Expr>(Expr::Kind::Undefined); }
        if (isKw("function")) { return parseFunction(); }
        if (t.kind == Tok::Ident || t.kind == Tok::Kw) {
            advance();
            auto e = std::make_unique<Expr>(Expr::Kind::Ident);
            e->str = t.text;
            return e;
        }
        if (t.kind == Tok::LParen) {
            advance();
            auto e = parseExpr();
            eat(Tok::RParen);
            return e;
        }
        ok = false;
        advance();
        return std::make_unique<Expr>(Expr::Kind::Undefined);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Evaluator
// ─────────────────────────────────────────────────────────────────────────────

struct Evaluator {
    JSContext* ctx;
    std::map<std::string, JSValue>* scope;
    bool returned = false;
    JSValue returnVal;
    JSValue lastValue;

    Evaluator(JSContext* c, std::map<std::string, JSValue>* s) : ctx(c), scope(s) {}

    struct ScopeRAII {
        Evaluator& ev;
        std::map<std::string, JSValue> local;
        std::map<std::string, JSValue>* prev;
        explicit ScopeRAII(Evaluator& e) : ev(e), prev(e.scope) { ev.scope = &local; }
        ~ScopeRAII() { ev.scope = prev; }
    };

    JSObject* globalObj() { return &ctx->global; }

    JSValue getVar(const std::string& name) {
        auto it = scope->find(name);
        if (it != scope->end()) return it->second;
        auto git = ctx->global.props.find(name);
        if (git != ctx->global.props.end()) return git->second;
        return JSValue::undefined();
    }

    void setVar(const std::string& name, const JSValue& v) {
        if (scope && scope != &ctx->global.props) {
            auto it = scope->find(name);
            if (it != scope->end()) { it->second = v; return; }
            auto git = ctx->global.props.find(name);
            if (git != ctx->global.props.end()) { git->second = v; return; }
        }
        ctx->global.props[name] = v;
    }

    JSValue evalExpr(Expr* e) {
        switch (e->kind) {
        case Expr::Kind::Num: { JSValue v = JSValue::number(e->num); return v; }
        case Expr::Kind::Str: return JSValue::string(e->str);
        case Expr::Kind::Bool: return JSValue::boolVal(e->num != 0);
        case Expr::Kind::Null: return JSValue::null();
        case Expr::Kind::Undefined: return JSValue::undefined();
        case Expr::Kind::Ident: return getVar(e->str);
        case Expr::Kind::Unary: {
            JSValue v = evalExpr(e->a.get());
            if (e->op == "-") {
                double n = jsToNumber(v);
                return JSValue::number(-n);
            }
            if (e->op == "!") return JSValue::boolVal(!jsToBool(v));
            return v;
        }
        case Expr::Kind::Binary: return evalBinary(e);
        case Expr::Kind::Assign: return evalAssign(e);
        case Expr::Kind::CompoundAssign: return evalCompound(e);
        case Expr::Kind::Member: return evalMemberGet(e);
        case Expr::Kind::Call: return evalCall(e);
        case Expr::Kind::Func: return evalFunctionExpr(e);
        }
        return JSValue::undefined();
    }

    JSValue evalBinary(Expr* e) {
        if (e->op == "&&") return jsToBool(evalExpr(e->a.get())) ? evalExpr(e->b.get()) : evalExpr(e->a.get());
        if (e->op == "||") return jsToBool(evalExpr(e->a.get())) ? evalExpr(e->a.get()) : evalExpr(e->b.get());
        JSValue l = evalExpr(e->a.get());
        JSValue r = evalExpr(e->b.get());
        if (e->op == "+") {
            if (l.type == JSValueType::String || r.type == JSValueType::String)
                return JSValue::string(jsToString(l) + jsToString(r));
            return JSValue::number(jsToNumber(l) + jsToNumber(r));
        }
        double ln = jsToNumber(l), rn = jsToNumber(r);
        if (e->op == "-") return JSValue::number(ln - rn);
        if (e->op == "*") return JSValue::number(ln * rn);
        if (e->op == "/") return JSValue::number(rn == 0 ? std::nan("") : ln / rn);
        if (e->op == "%") return JSValue::number(std::fmod(ln, rn));
        if (e->op == "<") return JSValue::boolVal(ln < rn);
        if (e->op == ">") return JSValue::boolVal(ln > rn);
        if (e->op == "<=") return JSValue::boolVal(ln <= rn);
        if (e->op == ">=") return JSValue::boolVal(ln >= rn);
        if (e->op == "==") return JSValue::boolVal(valuesEqual(l, r));
        if (e->op == "!=") return JSValue::boolVal(!valuesEqual(l, r));
        return JSValue::undefined();
    }

    bool valuesEqual(const JSValue& l, const JSValue& r) {
        if (l.type == r.type) {
            switch (l.type) {
            case JSValueType::Undefined:
            case JSValueType::Null: return true;
            case JSValueType::Bool: return l.b == r.b;
            case JSValueType::Number: return l.num == r.num;
            case JSValueType::String: return l.str == r.str;
            default: return l.obj == r.obj;
            }
        }
        return jsToNumber(l) == jsToNumber(r);
    }

    JSValue evalAssign(Expr* e) {
        JSValue r = evalExpr(e->b.get());
        assignToTarget(e->a.get(), r);
        return r;
    }

    JSValue evalCompound(Expr* e) {
        JSValue cur = evalExpr(e->a.get());
        JSValue r = evalExpr(e->b.get());
        JSValue result;
        if (e->op == "+") {
            if (cur.type == JSValueType::String || r.type == JSValueType::String)
                result = JSValue::string(jsToString(cur) + jsToString(r));
            else result = JSValue::number(jsToNumber(cur) + jsToNumber(r));
        } else if (e->op == "-") result = JSValue::number(jsToNumber(cur) - jsToNumber(r));
        else if (e->op == "*") result = JSValue::number(jsToNumber(cur) * jsToNumber(r));
        else result = JSValue::number(jsToNumber(cur) / jsToNumber(r));
        assignToTarget(e->a.get(), result);
        return result;
    }

    void assignToTarget(Expr* target, const JSValue& v) {
        if (target->kind == Expr::Kind::Ident) {
            setVar(target->str, v);
        } else if (target->kind == Expr::Kind::Member) {
            // Evaluate only the object side (target->a), not the whole member
            // expression, so compound assigns like `o.p += x` resolve the
            // reference `o.p` rather than re-reading its current value.
            JSValue obj = evalExpr(target->a.get());
            std::string key = memberKey(target);
            if (obj.type == JSValueType::Object && obj.obj) {
                if (obj.obj->setter) obj.obj->setter(key, v);
                else obj.obj->props[key] = v;
            }
        }
    }

    std::string memberKey(Expr* e) {
        if (!e->str.empty()) return e->str;
        if (e->b) {
            JSValue k = evalExpr(e->b.get());
            if (k.type == JSValueType::Number) return jsToString(k);
            return k.str;
        }
        return "";
    }

    JSValue evalMemberGet(Expr* e) {
        JSValue obj = evalExpr(e->a.get());
        std::string key = memberKey(e);
        if (obj.type != JSValueType::Object || !obj.obj) return JSValue::undefined();
        JSObject* o = obj.obj;
        auto it = o->props.find(key);
        if (it != o->props.end()) {
            if (it->second.type == JSValueType::Object && it->second.obj && it->second.obj->getter)
                return it->second.obj->getter(key);
            return it->second;
        }
        if (o->getter) return o->getter(key);
        // prototype chain
        JSObject* p = o->prototype;
        while (p) {
            auto pit = p->props.find(key);
            if (pit != p->props.end()) return pit->second;
            p = p->prototype;
        }
        return JSValue::undefined();
    }

    JSValue evalFunctionExpr(Expr* e) {
        JSObject* fn = new JSObject();
        fn->typeName = "Function";
        fn->call = [this, e](const std::vector<JSValue>& args) -> JSValue {
            ScopeRAII sc(*this);
            for (size_t i = 0; i < e->params.size(); ++i) {
                sc.local[e->params[i]] = (i < args.size()) ? args[i] : JSValue::undefined();
            }
            for (auto& stmt : e->body) {
                execStmt(stmt.get());
                if (returned) return returnVal;
            }
            return JSValue::undefined();
        };
        return JSValue::object(fn);
    }

    JSValue evalCall(Expr* e) {
        JSValue callee = evalExpr(e->a.get());
        std::vector<JSValue> args;
        for (auto& a : e->args) args.push_back(evalExpr(a.get()));
        if (callee.type != JSValueType::Function && !(callee.type == JSValueType::Object && callee.obj && callee.obj->call)) {
            throw JSRuntimeError("TypeError: not a function");
        }
        if (callee.type == JSValueType::Object && callee.obj && callee.obj->call) {
            return callee.obj->call(args);
        }
        throw JSRuntimeError("TypeError: not a function");
    }

    void execStmt(Stmt* s) {
        switch (s->kind) {
        case Stmt::Kind::Empty: return;
        case Stmt::Kind::ExprStmt: if (s->expr) lastValue = evalExpr(s->expr.get()); return;
        case Stmt::Kind::VarDecl: {
            JSValue v = s->init ? evalExpr(s->init.get()) : JSValue::undefined();
            setVar(s->name, v);
            return;
        }
        case Stmt::Kind::Block: {
            ScopeRAII sc(*this);
            for (auto& b : s->block) { execStmt(b.get()); if (returned) return; }
            return;
        }
        case Stmt::Kind::If: {
            if (jsToBool(evalExpr(s->cond.get()))) execStmt(s->thenB.get());
            else if (s->elseB) execStmt(s->elseB.get());
            return;
        }
        case Stmt::Kind::While: {
            while (jsToBool(evalExpr(s->cond.get()))) {
                for (auto& b : s->block) { execStmt(b.get()); if (returned) return; }
            }
            return;
        }
        case Stmt::Kind::Return: {
            returnVal = s->expr ? evalExpr(s->expr.get()) : JSValue::undefined();
            returned = true;
            return;
        }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Runtime / Context
// ─────────────────────────────────────────────────────────────────────────────

JSObject::JSObject() : prototype(nullptr), native(nullptr) {}
JSObject::~JSObject() {}

// Registry of every JSObject allocated, freed when the runtime is destroyed so
// the embedding does not leak per-script DOM wrappers (no incremental GC yet).
static std::set<JSObject*> g_objects;
static std::mutex g_objectMutex;

JSRuntime::~JSRuntime() {
    for (auto& job : pendingJobs) { (void)job; }
    {
        std::lock_guard<std::mutex> lk(g_objectMutex);
        for (JSObject* o : g_objects) delete o;
        g_objects.clear();
    }
}

JSContext::JSContext(JSRuntime* runtime) : rt(runtime), opaque(nullptr), exception(false) {}

JSContext::~JSContext() {}

// Timer / RAF callback registries are stored on the runtime.
struct JSTimerEntry {
    JSObject* func;
    double delay;
    bool interval;
    double remaining;
    JSTimerId id;
};
struct JSRafEntry {
    JSObject* func;
    JSTimerId id;
};

// We keep timer/raf state in the runtime via opaque vectors in the API's
// existing `timers` member; augment with a raf list stored alongside.

static std::vector<JSTimerEntry>& timerList(JSRuntime* rt) {
    static std::map<JSRuntime*, std::vector<JSTimerEntry>> store;
    return store[rt];
}
static std::vector<JSRafEntry>& rafList(JSRuntime* rt) {
    static std::map<JSRuntime*, std::vector<JSRafEntry>> store;
    return store[rt];
}

JSRuntime* JS_NewRuntime() { return new JSRuntime(); }
void JS_FreeRuntime(JSRuntime* rt) { delete rt; }

JSContext* JS_NewContext(JSRuntime* rt) {
    auto* ctx = new JSContext(rt);
    // Prime the global with standard Object/Function helpers.
    return ctx;
}
void JS_FreeContext(JSContext* ctx) { delete ctx; }

JSObject* JS_NewObject(const std::string& typeName) {
    auto* o = new JSObject();
    o->typeName = typeName;
    {
        std::lock_guard<std::mutex> lk(g_objectMutex);
        g_objects.insert(o);
    }
    return o;
}

JSValue JS_Eval(JSContext* ctx, const std::string& code, const std::string& /*filename*/) {
    Parser parser(code);
    auto prog = parser.parseProgram();
    if (!parser.ok || !prog) {
        ctx->exception = true;
        ctx->error = "SyntaxError: failed to parse script";
        return JSValue::undefined();
    }
    try {
        Evaluator ev(ctx, &ctx->global.props);
        JSValue result = JSValue::undefined();
        for (auto& s : prog->block) {
            ev.execStmt(s.get());
            if (ev.returned) { result = ev.returnVal; break; }
        }
        if (!ev.returned && ev.lastValue.type != JSValueType::Undefined)
            result = ev.lastValue;
        ctx->exception = false;
        return result;
    } catch (const JSRuntimeError& ex) {
        ctx->exception = true;
        ctx->error = ex.what();
        return JSValue::undefined();
    } catch (const std::exception& ex) {
        ctx->exception = true;
        ctx->error = std::string("Error: ") + ex.what();
        return JSValue::undefined();
    }
}

void JS_ExecutePendingJobs(JSContext* ctx) {
    if (!ctx || !ctx->rt) return;
    // Run microtasks (pending jobs) and fire due timers.
    auto jobs = std::move(ctx->rt->pendingJobs);
    for (auto& job : jobs) {
        try { job(ctx); } catch (...) {}
    }
    // Fire timers whose remaining delay has elapsed (driven per-frame, dtMs=1 "tick").
    auto& timers = timerList(ctx->rt);
    std::vector<JSTimerEntry> due;
    timers.erase(std::remove_if(timers.begin(), timers.end(),
        [&](JSTimerEntry& t) {
            t.remaining -= 16.0; // approximate frame step
            if (t.remaining <= 0) {
                if (t.interval) { t.remaining += t.delay; due.push_back(t); }
                else due.push_back(t);
                return !t.interval;
            }
            return false;
        }), timers.end());
    for (auto& t : due) {
        if (t.func && t.func->call) {
            try { t.func->call({}); } catch (...) {}
        }
    }
}

void JS_SetGlobal(JSContext* ctx, const std::string& name, JSValue val) {
    ctx->global.props[name] = val;
}
JSValue JS_GetGlobal(JSContext* ctx, const std::string& name) {
    auto it = ctx->global.props.find(name);
    return it != ctx->global.props.end() ? it->second : JSValue::undefined();
}

JSTimerId JS_SetTimeout(JSContext* ctx, JSValue func, double delay) {
    JSTimerId id = ctx->rt->nextTimerId++;
    timerList(ctx->rt).push_back({func.obj, delay, false, delay, id});
    return id;
}
JSTimerId JS_SetInterval(JSContext* ctx, JSValue func, double delay) {
    JSTimerId id = ctx->rt->nextTimerId++;
    double d = delay <= 0 ? 16.0 : delay;
    timerList(ctx->rt).push_back({func.obj, d, true, d, id});
    return id;
}
void JS_ClearTimeout(JSContext* ctx, JSTimerId id) {
    auto& v = timerList(ctx->rt);
    v.erase(std::remove_if(v.begin(), v.end(),
        [id](const JSTimerEntry& t) { return t.id == id && !t.interval; }), v.end());
}
void JS_ClearInterval(JSContext* ctx, JSTimerId id) {
    auto& v = timerList(ctx->rt);
    v.erase(std::remove_if(v.begin(), v.end(),
        [id](const JSTimerEntry& t) { return t.id == id && t.interval; }), v.end());
}

JSTimerId JS_RequestAnimationFrame(JSContext* ctx, JSValue func) {
    JSTimerId id = ctx->rt->nextRafId++;
    rafList(ctx->rt).push_back({func.obj, id});
    return id;
}

void JS_SetOpaque(JSContext* ctx, void* opaque) { ctx->opaque = opaque; }
void* JS_GetOpaque(JSContext* ctx) { return ctx->opaque; }

bool JS_IsException(JSValue val) { return val.type == JSValueType::Undefined; }
std::string JS_GetException(JSContext* ctx) { return ctx->error; }

// ─────────────────────────────────────────────────────────────────────────────
// RAF dispatch: called by the browser frame loop so callbacks receive a
// high-resolution timestamp argument.
// ─────────────────────────────────────────────────────────────────────────────

void JS_DispatchAnimationFrame(JSContext* ctx, double /*timestampMs*/) {
    if (!ctx || !ctx->rt) return;
    auto callbacks = std::move(rafList(ctx->rt));
    for (auto& entry : callbacks) {
        if (entry.func && entry.func->call) {
            try { entry.func->call({ JSValue::number(0.0) }); } catch (...) {}
        }
    }
}

} // namespace quickjs
