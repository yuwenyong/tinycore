//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_IOLOOP_H
#define TINYCORE_IOLOOP_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>


class _Timeout;
class PeriodicCallback;
typedef std::weak_ptr<_Timeout> Timeout;
typedef std::shared_ptr<PeriodicCallback> PeriodicCallbackPtr;


class TC_COMMON_API IOLoop {
public:
    typedef boost::asio::io_service ServiceType;
    typedef std::function<void()> CallbackType;
    typedef std::function<void()> TimeoutCallbackType;

    IOLoop(const IOLoop&) = delete;
    IOLoop& operator=(const IOLoop&) = delete;
    IOLoop();
    ~IOLoop();
    int start();
    Timeout addTimeout(float deadline, TimeoutCallbackType callback);
    void removeTimeout(Timeout timeout);

    void addCallback(CallbackType callback) {
        _ioService.post(std::move(callback));
    }

    bool running() const {
        return !_ioService.stopped();
    }

    void stop() {
        if (!_stopped) {
            _stopped = true;
            _ioService.stop();
        }
    }

    ServiceType& getService() {
        return _ioService;
    }

    static IOLoop* instance();
protected:
    ServiceType _ioService;
    volatile bool _stopped{false};
};


class TC_COMMON_API _Timeout {
public:
    friend class IOLoop;
    typedef boost::asio::steady_timer TimerType;
    typedef std::function<void (const ErrorCode&)> CallbackType;

    _Timeout(IOLoop* ioloop);
    ~_Timeout();
    _Timeout(const _Timeout &) = delete;
    _Timeout &operator=(const _Timeout &) = delete;
protected:
    void start(float deadline, CallbackType callback);
    void cancel();

    TimerType _timer;
};


class TC_COMMON_API PeriodicCallback: public std::enable_shared_from_this<PeriodicCallback> {
public:
    typedef IOLoop::TimeoutCallbackType CallbackType;

    PeriodicCallback(CallbackType callback, float callbackTime, IOLoop *ioloop)
            : _callback(std::move(callback))
            , _callbackTime(callbackTime), _ioloop(ioloop ? ioloop : IOLoop::instance())
            , _running(false) {

    }

    ~PeriodicCallback();

    void start() {
        _running = true;
        _ioloop->addTimeout(_callbackTime, std::bind(&PeriodicCallback::run, shared_from_this()));
    }

    void stop() {
        _running = false;
    }

    static PeriodicCallbackPtr create(CallbackType callback, float callbackTime, IOLoop *ioloop) {
        return std::make_shared<PeriodicCallback>(std::move(callback), callbackTime, ioloop);
    }
protected:
    void run();

    CallbackType _callback;
    float _callbackTime;
    IOLoop *_ioloop;
    bool _running;
};

#endif //TINYCORE_IOLOOP_H
