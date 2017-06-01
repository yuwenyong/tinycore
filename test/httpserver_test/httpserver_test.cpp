//
// Created by yuwenyong on 17-5-26.
//

#define BOOST_TEST_MODULE httpserver_test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/tinycore.h"


class HelloWorldRequestHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        BOOST_CHECK_EQUAL(_request->getProtocol(), "https");
        finish("Hello world");
    }

    void onPost(StringVector args) override {
        finish(String::format("Got %d bytes in POST", (int)_request->getBody().size()));
    }
};


class SSLTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloWorldRequestHandler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    std::shared_ptr<SSLOption> getHTTPServerSSLOption() const override {
        return SSLOption::createServerSide("test.crt", "test.key");
    }

    void testSSL() {
        std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
        std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, validateCert_=false);
        HTTPResponse response = fetch(std::move(request));
        const ByteArray *buffer = response.getBody();
        BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
        std::string body((const char*)buffer->data(), buffer->size());
        BOOST_CHECK_EQUAL(body, "Hello world");
    }

    void testLargePost() {
        std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
        ByteArray data;
        for (size_t i = 0; i != 5000; ++i) {
            data.push_back('A');
        }
        std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, validateCert_=false, method_="POST",
                                                                   body_=data);
        HTTPResponse response = fetch(std::move(request));
        const ByteArray *buffer = response.getBody();
        BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
        std::string body((const char*)buffer->data(), buffer->size());
        BOOST_CHECK_EQUAL(body, "Got 5000 bytes in POST");
    }

    void testNonSSLRequest() {
        HTTPResponse response = fetch(getURL("/"), requestTimeout_=3600, connectTimeout_=3600);
        BOOST_CHECK_EQUAL(response.getCode(), 599);
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_FIXTURE_TEST_CASE(TestSSL, TestCaseFixture<SSLTest>) {
    testCase.testSSL();
}

BOOST_FIXTURE_TEST_CASE(TestLargePost, TestCaseFixture<SSLTest>) {
    testCase.testLargePost();
}

BOOST_FIXTURE_TEST_CASE(TestNonSSLRequest, TestCaseFixture<SSLTest>) {
    testCase.testNonSSLRequest();
}