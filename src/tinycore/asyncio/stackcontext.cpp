//
// Created by yuwenyong on 17-6-16.
//

#include "tinycore/asyncio/stackcontext.h"
#include <boost/foreach.hpp>


std::function<void()> StackState::wrap(std::function<void()> callback) {
    if (!callback || _contexts.empty()) {
        return callback;
    }
    ExceptionHandlers contexts = _contexts;
    return [callback, contexts](){
        std::exception_ptr error;
        try {
            callback();
        } catch (...) {
            error = std::current_exception();
        }
        if (error) {
            handleException(contexts, error);
        }
    };
}


void StackState::handleException(const ExceptionHandlers &contexts, std::exception_ptr error) {
    BOOST_REVERSE_FOREACH(const ExceptionHandler &handler, contexts) {
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


thread_local ExceptionHandlers StackState::_contexts;