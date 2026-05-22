#ifndef JS_PROMISE_H
#define JS_PROMISE_H

#include "Value.h"
#include <functional>
#include <queue>
#include <memory>

namespace js {

enum class PromiseState {
    Pending,
    Fulfilled,
    Rejected
};

class Promise : public Object {
public:
    using Resolver = std::function<void(const Value&)>;
    using Rejecter = std::function<void(const Value&)>;
    using Callback = std::function<void(const Value&)>;
    
    Promise();
    Promise(std::function<void(Resolver, Rejecter)> executor);
    
    void resolve(const Value& value);
    void reject(const Value& reason);
    
    Promise* then(Callback on_fulfilled);
    Promise* catch_(Callback on_rejected);
    
    PromiseState state() const { return state_; }
    Value value() const { return value_; }
    Value reason() const { return reason_; }
    
private:
    PromiseState state_ = PromiseState::Pending;
    Value value_;
    Value reason_;
    
    std::vector<Callback> on_fulfilled_;
    std::vector<Callback> on_rejected_;
    
    void checkFinalState();
};

class MicrotaskQueue {
public:
    static MicrotaskQueue& instance();
    
    void enqueue(std::function<void()> task);
    void run();
    bool empty() const;
    
private:
    MicrotaskQueue() = default;
    std::queue<std::function<void()>> queue_;
};

Value awaitPromise(Promise* promise);

} // namespace js

#endif // JS_PROMISE_H