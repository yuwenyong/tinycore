//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/asyncio/ioloop.h"
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
            if (!(iter->second)()) {
                _signalSet.remove(signalNumber);
                _callbacks.erase(iter);
            }
        }
        if (_ioloop->running()) {
            start();
        }
    }
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

void _Timeout::start(const Timestamp &deadline, CallbackType callback) {
    _timer.expires_at(deadline);
    _timer.async_wait(std::move(callback));
}

void _Timeout::start(const Duration &deadline, CallbackType callback) {
    _timer.expires_from_now(deadline);
    _timer.async_wait(std::move(callback));
}

void _Timeout::start(float deadline, CallbackType callback) {
    _timer.expires_from_now(std::chrono::milliseconds(int64_t(deadline * 1000)));
    _timer.async_wait(std::move(callback));
}

void _Timeout::cancel() {
    _timer.cancel();
}



IOLoop::IOLoop()
        : _ioService()
        , _signalSet(this) {
    setupInterrupter();
}

IOLoop::~IOLoop() {

}

void IOLoop::start() {
    if (_stopped) {
        return;
    }
    if (_ioService.stopped()) {
        _ioService.reset();
    }
    WorkType work(_ioService);
    _signalSet.start();
    while (!_ioService.stopped()) {
        try {
            _ioService.run();
        } catch (SystemExit &e) {
            Log::error(e.what());
            break;
        } catch (std::exception &e) {
            Log::error("Unexpected Exception:%s", e.what());
        }
    }
    _stopped = false;
}

void IOLoop::removeTimeout(Timeout timeout) {
    auto timeoutHandle = timeout.lock();
    if (timeoutHandle) {
        timeoutHandle->cancel();
    }
}


void IOLoop::setupInterrupter() {
    signal(SIGINT, [this](){
        Log::info("Capture SIGINT");
        stop();
        return false;
    });
    signal(SIGTERM, [this](){
        Log::info("Capture SIGTERM");
        stop();
        return false;
    });
#if defined(SIGQUIT)
    signal(SIGQUIT, [this](){
        Log::info("Capture SIGQUIT");
        stop();
        return false;
    });
#endif
}


PeriodicCallback::PeriodicCallback(CallbackType callback, float callbackTime, IOLoop *ioloop)
        : PeriodicCallback(std::move(callback),
                           std::chrono::milliseconds(int64_t(callbackTime * 1000)),
                           ioloop) {
}

PeriodicCallback::PeriodicCallback(CallbackType callback, Duration callbackTime, IOLoop *ioloop)
        : _callback(std::move(callback))
        , _callbackTime(std::move(callbackTime))
        , _ioloop(ioloop ? ioloop : sIOLoop)
        , _running(false) {
#ifndef NDEBUG
    sWatcher->inc(SYS_PERIODICCALLBACK_COUNT);
#endif
}

PeriodicCallback::~PeriodicCallback() {
#ifndef NDEBUG
    sWatcher->dec(SYS_PERIODICCALLBACK_COUNT);
#endif
}

void PeriodicCallback::run() {
    if (!_running) {
        return;
    }
    try {
        _callback();
    } catch (std::exception &e) {
        Log::error("Error in periodic callback:%s", e.what());
    }
    scheduleNext();
}