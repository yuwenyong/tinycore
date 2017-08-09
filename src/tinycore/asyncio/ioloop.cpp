//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/stackcontext.h"
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
            LOG_ERROR(e.what());
            break;
        } catch (std::exception &e) {
            LOG_ERROR("Unexpected Exception:%s", e.what());
        }
    }
    _stopped = false;
}

void IOLoop::removeTimeout(Timeout timeout) {
    auto timeout_ = timeout.lock();
    if (timeout_) {
        timeout_->cancel();
    }
}

void IOLoop::addCallback(CallbackType callback) {
    callback = StackContext::wrap(std::move(callback));
    _ioService.post(std::move(callback));
}

_Timeout::CallbackType IOLoop::wrapTimeoutCallback(std::shared_ptr<_Timeout> timeout, TimeoutCallbackType callback) {
    callback = StackContext::wrap(std::move(callback));
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    return [callback = std::move(callback), timeout = std::move(timeout)](
            const boost::system::error_code &error) {
        if (!error) {
            callback();
        }
    };
#else
    return std::bind([timeout](TimeoutCallbackType &callback, const boost::system::error_code &error) {
        if (!error) {
            callback();
        }
    }, std::move(callback), std::placeholders::_1);
#endif
}

void IOLoop::setupInterrupter() {
    signal(SIGINT, [this](){
        LOG_INFO("Capture SIGINT");
        stop();
        return false;
    });
    signal(SIGTERM, [this](){
        LOG_INFO("Capture SIGTERM");
        stop();
        return false;
    });
#if defined(SIGQUIT)
    signal(SIGQUIT, [this](){
        LOG_INFO("Capture SIGQUIT");
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
        LOG_ERROR("Error in periodic callback:%s", e.what());
    }
    scheduleNext();
}