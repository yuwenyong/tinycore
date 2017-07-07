//
// Created by yuwenyong on 17-6-16.
//

#include "tinycore/asyncio/stackcontext.h"
#include <boost/foreach.hpp>


std::function<void()> StackContext::wrap(std::function<void()> &&callback) {
    if (!callback || callback.target_type() == typeid(_StackContextWrapper<>)) {
        return callback;
    }
    return _StackContextWrapper<>(_state.contexts, std::move(callback));
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