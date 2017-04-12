//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/networking/ioloop.h"
#include "tinycore/logging/log.h"
#include "tinycore/common/errors.h"


IOLoop::IOLoop() {

}

IOLoop::~IOLoop() {

}

int IOLoop::start() {
    if (_stopped) {
        return 0;
    }
    _ioService.run();
    _stopped = false;
    return 0;
}

Timeout IOLoop::addTimeout(float deadline, TimeoutCallbackType callback) {
    auto timeoutPtr = std::make_shared<_Timeout>(this);
    timeoutPtr->start(deadline, [callback, timeoutPtr](const ErrorCode &error) {
        if (!error) {
            callback();
        } else {
#ifndef NDEBUG
            std::string errorMessage = error.message();
            Log::error("ErrorCode:%d,ErrorMessage:%s", error.value(), errorMessage.c_str());
#endif
        }
    });
    return Timeout(timeoutPtr);
}


void IOLoop::removeTimeout(Timeout timeout) {
    auto timeoutPtr = timeout.lock();
    if (timeoutPtr) {
        timeoutPtr->cancel();
    }
}

IOLoop* IOLoop::instance() {
    static IOLoop ioloop;
    return &ioloop;
}


_Timeout::_Timeout(IOLoop *ioloop)
        : _timer(ioloop->getService()) {
//#ifndef NDEBUG
//    Log::info("Create timeout");
//#endif
}

_Timeout::~_Timeout() {
//#ifndef NDEBUG
//    Log::info("Destroy timeout");
//#endif
}

void _Timeout::start(float deadline, CallbackType callback) {
    _timer.expires_from_now(std::chrono::milliseconds(int64_t(deadline * 1000)));
    _timer.async_wait(std::move(callback));
}

void _Timeout::cancel() {
    _timer.cancel();
}


PeriodicCallback::~PeriodicCallback() {
    stop();
}

void PeriodicCallback::run() {
    if (!_running) {
        return;
    }
    try {
        _callback();
    } catch (SystemExit &e) {
        throw;
    } catch (std::exception &e) {
        Log::error("Error in periodic callback:%s", e.what());
    }
    if (_running) {
        start();
    }
}