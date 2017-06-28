//
// Created by yuwenyong on 17-6-16.
//

#ifndef TINYCORE_STACKCONTEXT_H
#define TINYCORE_STACKCONTEXT_H

#include "tinycore/common/common.h"
#include <boost/noncopyable.hpp>
#include <boost/scope_exit.hpp>


typedef std::function<void (std::exception_ptr)> ExceptionHandler;
typedef std::vector<ExceptionHandler> ContextState;


class StackContext {
public:
    friend class NullContext;

    static void push(ExceptionHandler exceptionHandler) {
        _state.emplace_back(std::move(exceptionHandler));
    }

    static void push(const ContextState &state) {
        _state.insert(_state.end(), state.begin(), state.end());
    }

    static void pop() {
        assert(!_state.empty());
        _state.pop_back();
    }

    static void pop(size_t count) {
        assert(_state.size() >= count);
        _state.erase(std::prev(_state.end(), count), _state.end());
    }

    static std::function<void()> wrap(std::function<void()> callback);
    static std::function<void(ByteArray)> wrap(std::function<void(ByteArray)> callback);

protected:
    static void handleException(const ContextState &state, std::exception_ptr error);

    static thread_local ContextState _state;
};


class ExceptionStackContext: public boost::noncopyable {
public:
    ExceptionStackContext(ExceptionHandler exceptionHandler) {
        StackContext::push(std::move(exceptionHandler));
    }

    ~ExceptionStackContext() {
        StackContext::pop();
    }
};


class NullContext: public boost::noncopyable {
public:
    NullContext() {
        _oldState.swap(StackContext::_state);
    }

    ~NullContext() {
        StackContext::_state.swap(_oldState);
    }
protected:
    ContextState _oldState;
};


#endif //TINYCORE_STACKCONTEXT_H
