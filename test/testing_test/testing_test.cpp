//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE testing_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class TestingTest: public AsyncTestCase {
public:
    void testExceptionInCallback() {
        _ioloop.addCallback([](){
            ThrowException(Exception, "TestException");
        });
        try {
            wait();
            BOOST_CHECK(false);
        } catch (std::exception &e) {

        }
    }
};


BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_FIXTURE_TEST_CASE(TestTesting, TestCaseFixture<TestingTest>) {
    testCase.testExceptionInCallback();
}