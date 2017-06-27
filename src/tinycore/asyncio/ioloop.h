//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_IOLOOP_H
#define TINYCORE_IOLOOP_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/logging/log.h"
#include "tinycore/utilities/objectmanager.h"


class IOLoop;

class _SignalSet {
public:
    typedef std::function<bool()> CallbackType;

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


class TC_COMMON_API _Timeout {
public:
    friend class IOLoop;
    typedef boost::asio::steady_timer TimerType;
    typedef std::function<void (const boost::system::error_code&)> CallbackType;

    _Timeout(IOLoop *ioloop);
    ~_Timeout();
    _Timeout(const _Timeout&) = delete;
    _Timeout& operator=(const _Timeout&) = delete;
protected:
    void start(const Timestamp &deadline, CallbackType callback);
    void start(const Duration &deadline, CallbackType callback);
    void start(float deadline, CallbackType callback);
    void cancel();

    TimerType _timer;
};

typedef std::weak_ptr<_Timeout> Timeout;


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
    void start();

    template <typename T>
    Timeout addTimeout(T &&deadline, TimeoutCallbackType callback) {
        auto timeout = std::make_shared<_Timeout>(this);
        callback = StackContext::wrap(std::move(callback));
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        auto func = [callback = std::move(callback), timeout = std::move(timeout)](
                const boost::system::error_code &error) {
            if (!error) {
                callback();
            }
        };
#else
        auto func = std::bind([timeout](TimeoutCallbackType &callback, const boost::system::error_code &error){
            if (!error) {
                callback();
            }
        }, std::move(callback), std::placeholders::_1);
#endif
        timeout->start(std::forward<T>(deadline), std::move(func));
        return Timeout(timeout);
    }

    void removeTimeout(Timeout timeout);

    void addCallback(CallbackType callback) {
        callback = StackContext::wrap(std::move(callback));
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
    void setupInterrupter();

    ServiceType _ioService;
    _SignalSet _signalSet;
    volatile bool _stopped{false};
};


#define sIOLoop Singleton<IOLoop>::instance()


class TC_COMMON_API PeriodicCallback: public std::enable_shared_from_this<PeriodicCallback> {
public:
    typedef IOLoop::TimeoutCallbackType CallbackType;

    PeriodicCallback(CallbackType callback, float callbackTime, IOLoop *ioloop= nullptr);
    PeriodicCallback(CallbackType callback, Duration callbackTime, IOLoop *ioloop= nullptr);
    ~PeriodicCallback();

    void start() {
        _running = true;
        scheduleNext();
    }

    void stop() {
        _running = false;
    }

    template <typename ...Args>
    static std::shared_ptr<PeriodicCallback> create(Args&& ...args) {
        return std::make_shared<PeriodicCallback>(std::forward<Args>(args)...);
    }
protected:
    void run();

    void scheduleNext() {
        if (_running) {
            _ioloop->addTimeout(_callbackTime, std::bind(&PeriodicCallback::run, shared_from_this()));
        }
    }

    CallbackType _callback;
    Duration _callbackTime;
    IOLoop *_ioloop;
    bool _running;
};


#endif //TINYCORE_IOLOOP_H
