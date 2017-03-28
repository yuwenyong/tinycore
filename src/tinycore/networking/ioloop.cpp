//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/networking/ioloop.h"


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
    TimeoutPtr timeout = std::make_shared<Timeout>(this);
    timeout->_start(deadline, [callback, timeout](const ErrorCode &error) {
        if (!error) {
            callback();
        }
    });
    return timeout;
}


void IOLoop::removeTimeout(TimeoutPtr &timeout) {
    if (timeout) {
        timeout->_cancel();
        timeout.reset();
    }
}

IOLoop* IOLoop::instance() {
    static IOLoop ioloop;
    return &ioloop;
}


Timeout::Timeout(IOLoop *ioloop)
        : _timer(ioloop->handle()) {
}

Timeout::~Timeout() {
}

void Timeout::_start(float deadline, std::function<void(const ErrorCode &)> callback) {
    _timer.expires_from_now(std::chrono::milliseconds(int(deadline * 1000)));
    _timer.async_wait(std::move(callback));
}


void Timeout::_cancel() {
    _timer.cancel();
}