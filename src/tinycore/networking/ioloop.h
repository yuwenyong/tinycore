//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_IOLOOP_H
#define TINYCORE_IOLOOP_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>


class Timeout {
public:
    friend class IOLoop;
    typedef boost::asio::steady_timer TimerType;

    Timeout(const Timeout &) = delete;
    Timeout &operator=(const Timeout &) = delete;

protected:
    Timeout(IOLoop * ioloop);
    void _start(float deadline, std::function<void (const ErrorCode&)> callback);
    void _cancel();

    TimerType _timer;
};


//typedef std::shared_ptr<Timeout> TimeoutPtr;

class TC_COMMON_API IOLoop {
public:
    typedef boost::asio::io_service ServiceType;

    IOLoop(const IOLoop&) = delete;
    IOLoop& operator=(const IOLoop&) = delete;
    IOLoop();
    ~IOLoop();
    int start();
    Timeout* addTimeout(float deadline, std::function<void()> callback);
    void removeTimeout(Timeout* timeout);

    void addCallback(std::function<void()> callback) {
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

    ServiceType& handle() {
        return _ioService;
    }

    static IOLoop* instance();
protected:
    ServiceType _ioService;
    volatile bool _stopped{false};
    std::set<Timeout*> _timeouts;
};

#endif //TINYCORE_IOLOOP_H
