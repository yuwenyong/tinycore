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
        wait();
    }

    void onConnect() {
        const char * data = "GET / HTTP/1.0\r\n\r\n";
        _stream->write((const Byte *)data, strlen(data));
        _stream->readBytes(9, std::bind(&IOStreamTest::onNormalRead, this, std::placeholders::_1,
                                        std::placeholders::_2));
    }

    void onNormalRead(Byte *data, size_t length) {
        std::string readString((char *)data, length);
        BOOST_REQUIRE_EQUAL(readString, "HTTP/1.0 ");
        _stream->readBytes(0, std::bind(&IOStreamTest::onZeroBytes, this, std::placeholders::_1,
                                        std::placeholders::_2));
    }

    void onZeroBytes(Byte *data, size_t length) {
        BOOST_REQUIRE_EQUAL(length, 0);
        stop();
//        _stream->readBytes(3, std::bind(&IOStreamTest::onNormalRead2, this, std::placeholders::_1,
//                                        std::placeholders::_2));
    }

    void onNormalRead2(Byte *data, size_t length) {
        std::string readString((char *)data, length);
        BOOST_REQUIRE_EQUAL(readString, "200");
        stop();
    }

protected:
    std::shared_ptr<IOStream> _stream;
};


BOOST_GLOBAL_FIXTURE(GlobalFixture);

BOOST_FIXTURE_TEST_CASE(TestIOStream, TestCaseFixture<IOStreamTest>) {
    testCase.testReadZeroBytes();
}