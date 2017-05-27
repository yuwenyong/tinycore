//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE httpclient_test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/tinycore.h"


class HelloWorld: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write("Hello world");
    }
};


class HelloWorldTestCase: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloWorld>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testHelloWorld() {
        fetch("/", [](const HTTPResponse &response) {
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            const ByteArray *buffer = response.getBody();
            BOOST_CHECK_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)buffer->data(), buffer->size());
            BOOST_CHECK_EQUAL(body, "Hello world");
        });
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_FIXTURE_TEST_CASE(TestHelloWorld, TestCaseFixture<HelloWorldTestCase>) {
    testCase.testHelloWorld();
}