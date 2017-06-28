//
// Created by yuwenyong on 17-6-16.
//

#include "tinycore/asyncio/stackcontext.h"
#include <boost/foreach.hpp>


std::function<void()> StackContext::wrap(std::function<void()> callback) {
    if (!callback || _state.empty()) {
        return callback;
    }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    auto func = [state=_state, callback=std::move(callback)](){
        StackContext::push(state);
        BOOST_SCOPE_EXIT(&state) {
            StackContext::pop(state.size());
        } BOOST_SCOPE_EXIT_END
        std::exception_ptr error;
        try {
            callback();
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(state, error);
        }
    };
#else
    auto func = std::bind([](ContextState &state, std::function<void ()> &callback){
            StackContext::push(state);
            BOOST_SCOPE_EXIT(&state) {
                StackContext::pop(state.size());
            } BOOST_SCOPE_EXIT_END
            std::exception_ptr error;
            try {
                callback();
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                StackContext::handleException(state, error);
            }
        }, _state, std::move(callback));
#endif
    return func;
}

std::function<void(ByteArray)> StackContext::wrap(std::function<void(ByteArray)> callback) {
    if (!callback || _state.empty()) {
        return callback;
    }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    auto func = [state=_state, callback=std::move(callback)](ByteArray arg){
        StackContext::push(state);
        BOOST_SCOPE_EXIT(&state) {
            StackContext::pop(state.size());
        } BOOST_SCOPE_EXIT_END
        std::exception_ptr error;
        try {
            callback(std::move(arg));
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            StackContext::handleException(state, error);
        }
    };
#else
    auto func = std::bind([](ContextState &state, std::function<void (ByteArray)> &callback, ByteArray arg){
            StackContext::push(state);
            BOOST_SCOPE_EXIT(&state) {
                StackContext::pop(state.size());
            } BOOST_SCOPE_EXIT_END
            std::exception_ptr error;
            try {
                callback(std::move(arg));
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                StackContext::handleException(state, error);
            }
        }, _state, std::move(callback), std::placeholders::_1);
#endif
    return func;
}

void StackContext::handleException(const ContextState &state, std::exception_ptr error) {
    BOOST_REVERSE_FOREACH(const ExceptionHandler &handler, state) {
        try {
            handler(error);
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

thread_local ContextState StackContext::_state;