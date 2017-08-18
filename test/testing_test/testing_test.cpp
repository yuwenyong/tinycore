//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE testing_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class TestingTest: public AsyncTestCase {
public:
    void testExceptionInCallback() {
        _ioloop.addCallback([]() {
            ThrowException(Exception, "TestException");
        });
        try {
            wait();
            BOOST_CHECK(false);
        } catch (std::exception &e) {
        }
    }

    void testSubsequentWaitCalls() {
        _ioloop.addTimeout(0.01f, [this]() {
            stop();
        });
        wait(0.02f);
        _ioloop.addTimeout(0.03f, [this]() {
            stop();
        });
        wait(0.15f);
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(TestingTest, testExceptionInCallback)
TINYCORE_TEST_CASE(TestingTest, testSubsequentWaitCalls)
