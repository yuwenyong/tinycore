//
// Created by yuwenyong on 17-6-16.
//

#ifndef TINYCORE_STACKCONTEXT_H
#define TINYCORE_STACKCONTEXT_H

#include "tinycore/common/common.h"
#include <boost/noncopyable.hpp>


typedef std::function<void (std::exception_ptr)> ExceptionHandler;
typedef std::vector<ExceptionHandler> ContextState;


class StackContext {
public:
    friend class NullContext;

    static void push(ExceptionHandler exceptionHandler) {
        _state.emplace_back(std::move(exceptionHandler));
    }

    static void pop() {
        assert(!_state.empty());
        _state.pop_back();
    }

    static std::function<void()> wrap(std::function<void()> callback);

    template<typename... Args>
    static std::function<void (Args...)> wrap(std::function<void (Args...)> callback) {
        if (!callback || _state.empty()) {
            return callback;
        }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        auto func = [state=_state, callback=std::move(callback)](Args... args){
            std::exception_ptr error;
            try {
                callback(std::forward<Args>(args)...);
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                handleException(state, error);
            }
        };
#else
        auto func = std::bind([](ContextState &state, std::function<void (Args...)> &callback, Args... args){
            std::exception_ptr error;
            try {
                callback(std::forward<Args>(args)...);
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                handleException(state, error);
            }
        }, _state, std::move(callback));
#endif
        return func;
    }
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
    ContextState _oldState
};


#endif //TINYCORE_STACKCONTEXT_H
