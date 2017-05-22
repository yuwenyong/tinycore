//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/networking/ioloop.h"
#include "tinycore/logging/log.h"
#include "tinycore/common/errors.h"
#include "tinycore/debugging/watcher.h"


_SignalSet::_SignalSet(IOLoop *ioloop)
        : _ioloop(ioloop ? ioloop : sIOLoop)
        , _signalSet(_ioloop->getService()) {

}

void _SignalSet::signal(int signalNumber, CallbackType callback) {
    if (callback) {
        _callbacks[signalNumber] = std::move(callback);
        _signalSet.add(signalNumber);
        if (_ioloop->running()) {
            start();
        }
    } else {
        _signalSet.remove(signalNumber);
        _callbacks.erase(signalNumber);
    }
}

void _SignalSet::start() {
    if (_running || _callbacks.empty()) {
        return;
    }
    _running = true;
    _signalSet.async_wait(std::bind(&_SignalSet::onSignal, this,  std::placeholders::_1, std::placeholders::_2));
}


void _SignalSet::onSignal(const boost::system::error_code &error, int signalNumber) {
    _running = false;
    if (!error) {
        auto iter = _callbacks.find(signalNumber);
        if (iter != _callbacks.end()) {
            auto retCode = (iter->second)();
            if (retCode < 0) {
                _signalSet.remove(signalNumber);
                _callbacks.erase(iter);
            }
        }
        if (_ioloop->running()) {
            start();
        }
    }
}


IOLoop::IOLoop()
        : _ioService()
        , _signalSet(this) {
    setupInterrupter();
}

IOLoop::~IOLoop() {

}

int IOLoop::start() {
    if (_stopped) {
        return 0;
    }
    _signalSet.start();
    _ioService.run();
    _stopped = false;
    return 0;
}

Timeout IOLoop::addTimeout(float deadline, TimeoutCallbackType callback) {
    auto timeoutPtr = std::make_shared<_Timeout>(this);
    timeoutPtr->start(deadline, [callback, timeoutPtr](const boost::system::error_code &error) {
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

void IOLoop::setupInterrupter() {
    signal(SIGINT, [this](){
        Log::info("Capture SIGINT");
        this->stop();
        return -1;
    });
    signal(SIGTERM, [this](){
        Log::info("Capture SIGTERM");
        this->stop();
        return -1;
    });
#if defined(SIGQUIT)
    signal(SIGQUIT, [this](){
        Log::info("Capture SIGQUIT");
        this->stop();
        return -1;
    });
#endif
}


_Timeout::_Timeout(IOLoop *ioloop)
        : _timer(ioloop->getService()) {
#ifndef NDEBUG
    sWatcher->inc(SYS_TIMEOUT_COUNT);
#endif
}

_Timeout::~_Timeout() {
#ifndef NDEBUG
    sWatcher->dec(SYS_TIMEOUT_COUNT);
#endif
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