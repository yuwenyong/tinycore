//
// Created by yuwenyong on 17-6-16.
//

#include "tinycore/asyncio/stackcontext.h"
#include <boost/foreach.hpp>


std::function<void()> StackContext::wrap(std::function<void()> callback) {
    if (!callback || _state.contexts.empty()) {
        return callback;
    }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    return [contexts = _state.contexts, callback = std::move(callback)]() {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback();
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    };
#else
    return std::bind([](ExceptionHandlers &contexts, std::function<void()> &callback) {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback();
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    }, _state.contexts, std::move(callback));
#endif
}

std::function<void(ByteArray)> StackContext::wrap(std::function<void(ByteArray)> callback) {
    if (!callback || _state.contexts.empty()) {
        return callback;
    }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    return [contexts = _state.contexts, callback = std::move(callback)](ByteArray arg) {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback(std::move(arg));
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    };
#else
    return std::bind([](ExceptionHandlers &contexts, std::function<void(ByteArray)> &callback, ByteArray arg) {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback(std::move(arg));
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    }, _state, std::move(callback), std::placeholders::_1);
#endif
}

std::function<void(HTTPResponse)> StackContext::wrap(std::function<void(HTTPResponse)> callback) {
    if (!callback || _state.contexts.empty()) {
        return callback;
    }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    return [contexts = _state.contexts, callback = std::move(callback)](HTTPResponse arg) {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback(std::move(arg));
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    };
#else
    return std::bind([](ExceptionHandlers &contexts, std::function<void(HTTPResponse)> &callback, HTTPResponse arg) {
        StackContextSaver saver(contexts);
        std::exception_ptr error;
        try {
            callback(std::move(arg));
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(contexts, error);
        }
    }, _state, std::move(callback), std::placeholders::_1);
#endif
}

void StackContext::handleException(const ExceptionHandlers &contexts, std::exception_ptr error) {
    BOOST_REVERSE_FOREACH(const ExceptionHandler &context, contexts) {
        try {
            context(error);
            error = nullptr;
            break;
        } catch (...) {
            error = std::current_exception();
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

thread_local _State StackContext::_state;