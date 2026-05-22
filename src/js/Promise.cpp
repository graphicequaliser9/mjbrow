#include "js/Promise.h"

namespace js {

Promise::Promise() = default;

Promise::Promise(std::function<void(Resolver, Rejecter)> executor) {
    executor([this](const Value& v) { resolve(v); },
             [this](const Value& r) { reject(r); });
}

void Promise::resolve(const Value& v) {
    if (state_ != PromiseState::Pending) return;
    
    state_ = PromiseState::Fulfilled;
    value_ = v;
    
    for (auto& cb : on_fulfilled_) {
        MicrotaskQueue::instance().enqueue([cb, v]() {
            cb(v);
        });
    }
}

void Promise::reject(const Value& reason) {
    if (state_ != PromiseState::Pending) return;
    
    state_ = PromiseState::Rejected;
    reason_ = reason;
    
    for (auto& cb : on_rejected_) {
        MicrotaskQueue::instance().enqueue([cb, reason]() {
            cb(reason);
        });
    }
}

Promise* Promise::then(Callback on_fulfilled) {
    auto p = new Promise();
    
    if (state_ == PromiseState::Fulfilled) {
        MicrotaskQueue::instance().enqueue([p, on_fulfilled, v = value_]() {
            Value result = on_fulfilled(v);
            // Would need to resolve p with result
        });
    } else if (state_ == PromiseState::Rejected) {
        // Handle rejection
    } else {
        on_fulfilled_.push_back([p, on_fulfilled](const Value& v) {
            Value result = on_fulfilled(v);
            // Would need to resolve p with result
        });
    }
    
    return p;
}

Promise* Promise::catch_(Callback on_rejected) {
    auto p = new Promise();
    
    if (state_ == PromiseState::Rejected) {
        MicrotaskQueue::instance().enqueue([p, on_rejected, r = reason_]() {
            on_rejected(r);
        });
    } else if (state_ == PromiseState::Pending) {
        on_rejected_.push_back([on_rejected](const Value& r) {
            on_rejected(r);
        });
    }
    
    return p;
}

MicrotaskQueue& MicrotaskQueue::instance() {
    static MicrotaskQueue instance;
    return instance;
}

void MicrotaskQueue::enqueue(std::function<void()> task) {
    queue_.push(task);
}

void MicrotaskQueue::run() {
    while (!queue_.empty()) {
        auto task = queue_.front();
        queue_.pop();
        task();
    }
}

bool MicrotaskQueue::empty() const {
    return queue_.empty();
}

Value awaitPromise(Promise* promise) {
    // In a real implementation, this would yield to the microtask queue
    return Value::undefined();
}

} // namespace js