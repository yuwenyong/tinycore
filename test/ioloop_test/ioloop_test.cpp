//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE ioloop_test
#include <boost/test/included/unit_test.hpp>
#include <thread>
#include "tinycore/tinycore.h"


class IOLoopTest: public AsyncTestCase {
public:
    void testAddCallbackWakeup() {
        _ioloop.addTimeout(0.0f, [this](){
            _ioloop.addCallback([this](){
                _called = true;
                stop();
            });
            _startTime = TimestampClock::now();
        });
        wait();
        auto dura = std::chrono::duration_cast<std::chrono::milliseconds>(TimestampClock::now() - _startTime);
        BOOST_CHECK_LT(dura.count(), 10);
        BOOST_CHECK(_called);
    }

    void testAddCallbackWakeupOtherThread() {
        std::thread thread;
        _ioloop.addCallback([&thread, this]() {
            std::thread target([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                _stopTime = TimestampClock::now();
               _ioloop.addCallback([this](){
                   stop();
               });
            });
            thread.swap(target);
        });
        wait();
        auto dura = std::chrono::duration_cast<std::chrono::milliseconds>(TimestampClock::now() - _stopTime);
        BOOST_CHECK_LT(dura.count(), 10);
        thread.join();
    }

    void testAddTimeoutTimedelta() {
        _ioloop.addTimeout(std::chrono::microseconds(1), [this]() {
            stop();
        });
        wait();
    }

    void testRemoveTimeoutAfterFire() {
        auto handle = _ioloop.addTimeout(TimestampClock::now(), [this]() {
            stop();
        });
        wait();
        _ioloop.removeTimeout(handle);
    }

    void testRemoveTimeoutCleanup() {
        for (size_t i = 0; i < 2000; ++i) {
            auto timeout = _ioloop.addTimeout(3600.0f, [](){});
            _ioloop.removeTimeout(timeout);
        }
        _ioloop.addCallback([this]() {
            _ioloop.addCallback([this]() {
                stop();
            });
        });
        wait();
    }
protected:
    Timestamp _startTime;
    Timestamp _stopTime;
    bool _called{false};
};


class IOLoopCurrentTest: public TestCase {
public:
    void testCurrent() {
        _ioloop.addCallback([this]() {
            _currentIOLoop = IOLoop::current();
            _ioloop.stop();
        });
        _ioloop.start();
//        std::cerr << "SUCCESS" << std::endl;
        BOOST_CHECK_EQUAL(&_ioloop, _currentIOLoop);
    }

protected:
    IOLoop _ioloop;
    IOLoop *_currentIOLoop{nullptr};
};


class IOLoopRunSyncTest: public TestCase {
public:
    void testSyncResult() {
        int value = 0;
        _ioloop.runSync([&]() mutable {
            value = 42;
            _ioloop.stop();
        });
        BOOST_CHECK_EQUAL(value, 42);
    }
protected:
    IOLoop _ioloop;
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(IOLoopTest, testAddCallbackWakeup)
TINYCORE_TEST_CASE(IOLoopTest, testAddCallbackWakeupOtherThread)
TINYCORE_TEST_CASE(IOLoopTest, testAddTimeoutTimedelta)
TINYCORE_TEST_CASE(IOLoopTest, testRemoveTimeoutAfterFire)
TINYCORE_TEST_CASE(IOLoopTest, testRemoveTimeoutCleanup)
TINYCORE_TEST_CASE(IOLoopCurrentTest, testCurrent)
TINYCORE_TEST_CASE(IOLoopRunSyncTest, testSyncResult)
