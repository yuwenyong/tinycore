//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/networking/ioloop.h"
#include "tinycore/logging/log.h"


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

TimeoutPtr IOLoop::addTimeout(float deadline, std::function<void()> callback) {
    std::shared_ptr<Timeout> timer = std::make_shared<Timeout>(this);
    timer->_start(deadline, [callback, timer](const ErrorCode &error) {
        if (!error) {
            callback();
        } else {
#ifndef NDEBUG
            std::string errorMessage = error.message();
            Log::error("ErrorCode:%d,ErrorMessage:%s", error.value(), errorMessage.c_str());
#endif
        }
    });
    return TimeoutPtr(timer);
}


void IOLoop::removeTimeout(TimeoutPtr timeout) {
    auto timer = timeout.lock();
    if (timer) {
        timer->_cancel();
    }
}

IOLoop* IOLoop::instance() {
    static IOLoop ioloop;
    return &ioloop;
}


Timeout::Timeout(IOLoop *ioloop)
        : _timer(ioloop->getService()) {
//#ifndef NDEBUG
//    Log::info("Create timeout");
//#endif
}

Timeout::~Timeout() {
//#ifndef NDEBUG
//    Log::info("Destroy timeout");
//#endif
}

void Timeout::_start(float deadline, std::function<void(const ErrorCode &)> callback) {
    _timer.expires_from_now(std::chrono::milliseconds(int(deadline * 1000)));
    _timer.async_wait(std::move(callback));
}


void Timeout::_cancel() {
    _timer.cancel();
}