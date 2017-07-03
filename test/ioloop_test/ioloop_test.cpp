//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE ioloop_test
#include <boost/test/included/unit_test.hpp>
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

    void testAddTimeoutTimedelta() {
        _ioloop.addTimeout(std::chrono::microseconds(1), [this]() {
            stop();
        });
        wait();
    }
protected:
    Timestamp _startTime;
    bool _called{false};
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(IOLoopTest, testAddCallbackWakeup)
TINYCORE_TEST_CASE(IOLoopTest, testAddTimeoutTimedelta)
