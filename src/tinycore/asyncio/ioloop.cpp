//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/asyncio/ioloop.h"
#include "tinycore/common/errors.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"


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



IOLoop::IOLoop()
        : _ioService()
        , _signalSet(this) {

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

Timeout IOLoop::addTimeout(float deadline, TimeoutCallbackType callback) {
    auto timeoutPtr = std::make_shared<_Timeout>(this);
    timeoutPtr->start(deadline, [callback, timeoutPtr](const boost::system::error_code &error) {
        if (!error) {
            callback();
        } else {
#ifndef NDEBUG
            if (error != boost::asio::error::operation_aborted) {
                std::string errorMessage = error.message();
                Log::error("ErrorCode:%d,ErrorMessage:%s", error.value(), errorMessage.c_str());
            }
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


PeriodicCallback::PeriodicCallback(CallbackType callback, float callbackTime, IOLoop *ioloop)
        : _callback(std::move(callback))
        , _callbackTime(callbackTime)
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
    } catch (SystemExit &e) {
        throw;
    } catch (std::exception &e) {
        Log::error("Error in periodic callback:%s", e.what());
    }
    if (_running) {
        start();
    }
}