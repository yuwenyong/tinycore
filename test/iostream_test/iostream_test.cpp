//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE iostream_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class HelloHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write("Hello");
    }
};


class IOStreamTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloHandler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testReadZeroBytes() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->connect("localhost", getHTTPPort(), std::bind(&IOStreamTest::onConnect, this));
    }

    void onConnect() {
        const char * data = "GET / HTTP/1.0\r\n\r\n";
        _stream->write((const Byte *)data, strlen(data));

        // normal read
        do  {
            _stream->readBytes(9, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "HTTP/1.0 ");
        } while (false);

        // zero bytes
        do {
            _stream->readBytes(0, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            BOOST_REQUIRE_EQUAL(bytes.size(), 0);
        } while (false);

        // another normal read
        do {
            _stream->readBytes(3, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "200");
        } while (false);
    }

    void testConnectionRefused() {
        bool connectCalled = false;
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->setCloseCallback([this](){
            stop();
        });
        _stream->connect("localhost", getUnusedPort(), [&connectCalled](){
            connectCalled = true;
        });
        wait();
        BOOST_CHECK(!connectCalled);
    }

    void testConnectionClosed() {
        HTTPHeaders headers;
        headers["Connection"] = "close";
        HTTPResponse response = fetch("/", headers_=headers);
        response.rethrow();
    }
protected:
    std::shared_ptr<IOStream> _stream;
};


BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_FIXTURE_TEST_CASE(TestIOStream, TestCaseFixture<IOStreamTest>) {
    testCase.testReadZeroBytes();
}

BOOST_FIXTURE_TEST_CASE(TestConnectionRefused, TestCaseFixture<IOStreamTest>) {
    testCase.testConnectionRefused();
}

BOOST_FIXTURE_TEST_CASE(TestConnectionClosed, TestCaseFixture<IOStreamTest>) {
    testCase.testConnectionClosed();
}