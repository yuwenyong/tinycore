//
// Created by yuwenyong on 17-6-16.
//

#ifndef TINYCORE_STACKCONTEXT_H
#define TINYCORE_STACKCONTEXT_H

#include "tinycore/common/common.h"
#include "tinycore/asyncio/httpclient.h"


using ExceptionHandler = std::function<void (std::exception_ptr)>;
using ExceptionHandlers = std::vector<ExceptionHandler>;


class _State {
public:
    ExceptionHandlers contexts;
};


class StackContext {
public:
    static std::function<void()> wrap(std::function<void()> callback);
    static std::function<void(ByteArray)> wrap(std::function<void(ByteArray)> callback);
    static std::function<void(HTTPResponse)> wrap(std::function<void(HTTPResponse)> callback);
    static void handleException(const ExceptionHandlers &contexts, std::exception_ptr error);

    static thread_local _State _state;
};


class StackContextSaver {
public:
    StackContextSaver(ExceptionHandlers contexts)
            : _oldContexts(std::move(contexts)) {
        _oldContexts.swap(StackContext::_state.contexts);
    }

    ~StackContextSaver() {
        StackContext::_state.contexts.swap(_oldContexts);
    }
protected:
    ExceptionHandlers _oldContexts;
};


template <typename... Args>
class _StackContextWrapper {
public:
    _StackContextWrapper(ExceptionHandlers contexts, std::function<void(Args...)> callback)
            : _contexts(std::move(contexts))
            , _callback(std::move(callback)) {

    }

    void operator()(Args&&... args) {
        StackContextSaver saver(_contexts);
        if (_contexts.empty()) {
            _callback(std::forward<Args>(args)...);
            return;
        }
        std::exception_ptr error;
        try {
            _callback(std::forward<Args>(args)...);
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(_contexts, error);
        }
    }
protected:
    ExceptionHandlers _contexts;
    std::function<void(Args...)> _callback;
};


class ExceptionStackContext {
public:
    ExceptionStackContext(ExceptionHandler exceptionHandler) {
        StackContext::_state.contexts.emplace_back(std::move(exceptionHandler));
    }

    ~ExceptionStackContext() {
        StackContext::_state.contexts.pop_back();
    }
};


class NullContext {
public:
    NullContext() {
        _oldContexts.swap(StackContext::_state.contexts);
    }

    ~NullContext() {
        StackContext::_state.contexts.swap(_oldContexts);
    }
protected:
    ExceptionHandlers _oldContexts;
};


#endif //TINYCORE_STACKCONTEXT_H
