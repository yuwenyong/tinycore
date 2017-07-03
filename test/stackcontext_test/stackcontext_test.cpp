//
// Created by yuwenyong on 17-7-2.
//

#define BOOST_TEST_MODULE stackcontext_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class TestRequestHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override {
        _ioloop = boost::any_cast<IOLoop *>(args["ioloop"]);
    }

    void onGet(StringVector args) override {
        Asynchronous()
        Log::info("in get()");
        _ioloop->addCallback(std::bind(&TestRequestHandler::part2, this));
    }

    void part2() {
        Log::info("in part2()");
        _ioloop->addCallback(std::bind(&TestRequestHandler::part3, this));
    }

    void part3() {
        Log::info("in part3()");
        ThrowException(Exception, "test exception");
    }

    void writeError(int statusCode = 500, std::exception_ptr error= nullptr) override {
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (Exception &e) {
//                Log::error(e.what());
                if (boost::contains(e.what(), "test exception")) {
                    finish("got expected exception");
                } else {
                    finish("unexpected failure");
                }
            }
        } else {
            finish("unexpected failure");
        }
    }
protected:
    IOLoop *_ioloop{nullptr};
};


class HTTPStackContextTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {

        RequestHandler::ArgsType args = {
                {"ioloop", const_cast<IOLoop *>(&_ioloop)}
        };

        Application::HandlersType handlers = {
                url<TestRequestHandler>("/", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testStackContext() {
        _httpClient->fetch(getURL("/"), std::bind(&HTTPStackContextTest::handleResponse, this, std::placeholders::_1));
        wait();
        BOOST_CHECK_EQUAL(_response.getCode(), 500);
        const ByteArray *responseBody = _response.getBody();
        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
        std::string body((const char*)responseBody->data(), responseBody->size());
        BOOST_CHECK_EQUAL(body, "got expected exception");
//        Log::error("SUCCESS");
    }

    void handleResponse(HTTPResponse response) {
        _response = std::move(response);
        stop();
    }

protected:
    HTTPResponse _response;
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(HTTPStackContextTest, testStackContext)