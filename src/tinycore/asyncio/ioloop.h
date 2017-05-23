//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_IOLOOP_H
#define TINYCORE_IOLOOP_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "tinycore/utilities/objectmanager.h"


class IOLoop;
class _Timeout;
typedef std::weak_ptr<_Timeout> Timeout;

class PeriodicCallback;
typedef std::shared_ptr<PeriodicCallback> PeriodicCallbackPtr;

class _SignalSet {
public:
    typedef std::function<int ()> CallbackType;

    _SignalSet(IOLoop *ioloop= nullptr);

    void signal(int signalNumber, CallbackType callback=nullptr);
    void start();
protected:
    void onSignal(const boost::system::error_code &error, int signalNumber);

    IOLoop *_ioloop;
    boost::asio::signal_set _signalSet;
    bool _running{false};
    std::map<int, CallbackType> _callbacks;
};


class TC_COMMON_API IOLoop {
public:
    typedef boost::asio::io_service ServiceType;
    typedef ServiceType::work WorkType;
    typedef std::function<void()> CallbackType;
    typedef std::function<void()> TimeoutCallbackType;
    typedef _SignalSet::CallbackType SignalCallbackType;

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

    void signal(int signalNumber, SignalCallbackType callback= nullptr) {
        _signalSet.signal(signalNumber, std::move(callback));
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
protected:
    ServiceType _ioService;
    _SignalSet _signalSet;
    volatile bool _stopped{false};
};

#define sIOLoop Singleton<IOLoop>::instance()


class TC_COMMON_API _Timeout {
public:
    friend class IOLoop;
    typedef boost::asio::steady_timer TimerType;
    typedef std::function<void (const boost::system::error_code&)> CallbackType;

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

    PeriodicCallback(CallbackType callback, float callbackTime, IOLoop *ioloop= nullptr)
            : _callback(std::move(callback))
            , _callbackTime(callbackTime), _ioloop(ioloop ? ioloop : sIOLoop)
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

    static PeriodicCallbackPtr create(CallbackType callback, float callbackTime, IOLoop *ioloop= nullptr) {
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
