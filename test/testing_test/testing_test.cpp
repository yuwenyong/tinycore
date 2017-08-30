//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE testing_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class EnvironSetter {
public:
    EnvironSetter(std::string name, std::string value)
            : _name(std::move(name))
            , _value(std::move(value)) {
        setenv(_name.c_str(), _value.c_str(), 1);
    }

    ~EnvironSetter() {
        unsetenv(_name.c_str());
    }
protected:
    std::string _name;
    std::string _value;
};


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

    void testWaitTimeout() {
        _ioloop.addTimeout(0.01f, [this]() {
            stop();
        });
        wait();

        _ioloop.addTimeout(1.0f, [this]() {
            stop();
        });

        try {
            wait(0.01f);
            BOOST_CHECK(false);
        } catch (std::exception &e) {

        }

        _ioloop.addTimeout(1.0f, [this]() {
            stop();
        });
        EnvironSetter setter("ASYNC_TEST_TIMEOUT", "0.01");
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
TINYCORE_TEST_CASE(TestingTest, testWaitTimeout)
TINYCORE_TEST_CASE(TestingTest, testSubsequentWaitCalls)
