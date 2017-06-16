//
// Created by yuwenyong on 17-6-16.
//

#ifndef TINYCORE_STACKCONTEXT_H
#define TINYCORE_STACKCONTEXT_H

#include "tinycore/common/common.h"
#include <boost/noncopyable.hpp>


typedef std::function<void (std::exception_ptr)> ExceptionHandler;
typedef std::vector<ExceptionHandler> ExceptionHandlers;


class StackState {
public:
    friend class NullContext;

    static void push(ExceptionHandler exceptionHandler) {
        _contexts.emplace_back(std::move(exceptionHandler));
    }

    static void pop() {
        assert(!_contexts.empty());
        _contexts.pop_back();
    }

    static std::function<void()> wrap(std::function<void()> callback);

    template<typename... Args>
    static std::function<void (Args...)> wrap(std::function<void (Args...)> callback) {
        if (!callback || _contexts.empty()) {
            return callback;
        }
        ExceptionHandlers contexts = _contexts;
        return [callback, contexts](Args... args){
            std::exception_ptr error;
            try {
                callback(std::forward<Args>(args)...);
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                handleException(contexts, error);
            }
        };
    }
protected:
    static void handleException(const ExceptionHandlers &contexts, std::exception_ptr error);

    static thread_local ExceptionHandlers _contexts;
};


class ExceptionStackContext: public boost::noncopyable {
public:
    ExceptionStackContext(ExceptionHandler exceptionHandler) {
        StackState::push(std::move(exceptionHandler));
    }

    ~ExceptionStackContext() {
        StackState::pop();
    }
};


class NullContext: public boost::noncopyable {
public:
    NullContext() {
        StackState::_contexts.swap(_oldContexts);
    }

    ~NullContext() {
        StackState::_contexts.swap(_oldContexts);
    }
protected:
    ExceptionHandlers _oldContexts
};


#endif //TINYCORE_STACKCONTEXT_H
