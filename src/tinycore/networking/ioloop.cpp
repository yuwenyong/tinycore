//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/networking/ioloop.h"
#include <boost/checked_delete.hpp>
#include <boost/functional/factory.hpp>


Timeout::Timeout(IOLoop *ioloop)
        : _timer(ioloop->handle()) {

}

void Timeout::_start(float deadline, std::function<void(const ErrorCode &)> callback) {
    _timer.expires_from_now(std::chrono::milliseconds(int(deadline * 1000)));
    _timer.async_wait(std::move(callback));
}


void Timeout::_cancel() {
    _timer.cancel();
}


IOLoop::IOLoop() {

}

IOLoop::~IOLoop() {
    for (auto &timeout: _timeouts) {
        boost::checked_delete(timeout);
    }
    _timeouts.clear();
}

int IOLoop::start() {
    if (_stopped) {
        return 0;
    }
    _ioService.run();
    _stopped = false;
    return 0;
}

Timeout* IOLoop::addTimeout(float deadline, std::function<void()> callback) {
    Timeout* timeout = boost::factory<Timeout*>()(this);
    timeout->_start(deadline, [this, timeout, callback](const ErrorCode &error) {
        if (!error) {
            callback();
        }
        boost::checked_delete(timeout);
        _timeouts.erase(timeout);
    });
    _timeouts.insert(timeout);
    return timeout;
}


void IOLoop::removeTimeout(Timeout* timeout) {
    auto iter = _timeouts.find(timeout);
    if (iter != _timeouts.end()) {
        (*iter)->_cancel();
    }
}

IOLoop* IOLoop::instance() {
    static IOLoop ioloop;
    return &ioloop;
}