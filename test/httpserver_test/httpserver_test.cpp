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

    void initialize(ArgsType &args) override {
        _expectedProtocol = boost::any_cast<std::string>(args["protocol"]);
        std::cerr << _expectedProtocol << std::endl;
    }

    void onGet(StringVector args) override {
        BOOST_CHECK_EQUAL(_request->getProtocol(), _expectedProtocol);
        finish("Hello world");
    }

    void onPost(StringVector args) override {
        finish(String::format("Got %d bytes in POST", (int)_request->getBody().size()));
    }

protected:
    std::string _expectedProtocol;
};


class SSLTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {

        RequestHandler::ArgsType args = {
                {"protocol", std::string("https") }
        };

        Application::HandlersType handlers = {
                url<HelloWorldRequestHandler>("/", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    std::shared_ptr<SSLOption> getHTTPServerSSLOption() const override {
        auto sslOption = SSLOption::create(true);
        sslOption->setKeyFile("test.key");
        sslOption->setCertFile("test.crt");
        return sslOption;
    }

    void testSSL() {
        std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
        std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false);
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
        std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false, ARG_method="POST",
                                                                   ARG_body=data);
        HTTPResponse response = fetch(std::move(request));
        const ByteArray *buffer = response.getBody();
        BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
        std::string body((const char*)buffer->data(), buffer->size());
        BOOST_CHECK_EQUAL(body, "Got 5000 bytes in POST");
    }

    void testNonSSLRequest() {
        HTTPResponse response = fetch("/", ARG_requestTimeout=3600, ARG_connectTimeout=3600);
        BOOST_CHECK_EQUAL(response.getCode(), 599);
    }
};

TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(SSLTest, testSSL)
TINYCORE_TEST_CASE(SSLTest, testLargePost)
TINYCORE_TEST_CASE(SSLTest, testNonSSLRequest)
