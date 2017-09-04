//
// Created by yuwenyong on 17-8-18.
//

#define BOOST_TEST_MODULE websocket_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_TEST_DONT_PRINT_LOG_VALUE(ByteArray)

class WebSocketTest;


class TestWebSocketHandler: public WebSocketHandler {
public:
    using WebSocketHandler::WebSocketHandler;

    void initialize(ArgsType &args) override {
        _test = boost::any_cast<WebSocketTest *>(args["test"]);
    }

//    void onOpen(const StringVector &args) override {
//        std::cerr << "On Open" << std::endl;
//    }

    void onClose() override;

protected:
    WebSocketTest *_test{nullptr};
};


class EchoHandler: public TestWebSocketHandler {
public:
    using TestWebSocketHandler::TestWebSocketHandler;

    void onMessage(ByteArray data) override {
//        std::cerr << "On Message" << std::endl;
        writeMessage(data);
    }
};


class HeaderHandler: public TestWebSocketHandler {
public:
    using TestWebSocketHandler::TestWebSocketHandler;

    void onOpen(const StringVector &args) override {
        writeMessage(_request->getHTTPHeaders()->get("X-Test", ""));
    }
};


class NonWebSocketHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        write("ok");
    }
};


using WebSocketConnectionPtr = std::shared_ptr<WebSocketClientConnection>;


class WebSocketTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        RequestHandler::ArgsType args = {
                {"test", const_cast<WebSocketTest *>(this) }
        };
        Application::HandlersType handlers = {
                url<EchoHandler>("/echo", args),
                url<NonWebSocketHandler>("/non_ws"),
                url<HeaderHandler>("/header", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testWebSocketCallbacks() {
        WebSocketConnect(String::format("ws://localhost:%d/echo", getHTTPPort()),
                         [this](WebSocketConnectionPtr connection) {
//                             std::cerr << "On Connect" << std::endl;
                             stop(std::move(connection));
                         });
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        ws->writeMessage("Hello");
        ws->readMessage([this](boost::optional<ByteArray> data) {
//            std::cerr << "On Read" << std::endl;
            BOOST_REQUIRE_NE(data.get_ptr(), static_cast<const ByteArray *>(nullptr));
            stop(std::move(data.get()));
        });
        ByteArray response = wait<ByteArray>();
        BOOST_CHECK_EQUAL(String::toString(response), "Hello");
        ws->getStream()->close();
        std::string closed = wait<std::string>();
        BOOST_CHECK_EQUAL(closed, "closed");
    }

    void testWebSocketHTTPFail() {
        WebSocketConnect(String::format("ws://localhost:%d/notfound", getHTTPPort()),
                         [this](WebSocketConnectionPtr connection) {
                             stop(std::move(connection));
                         });
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        BOOST_CHECK(ws->getError());
        try {
            std::rethrow_exception(ws->getError());
        } catch (_HTTPError &e) {
            BOOST_CHECK_EQUAL(e.getCode(), 404);
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testWebSocketHTTPSuccess() {
        WebSocketConnect(String::format("ws://localhost:%d/non_ws", getHTTPPort()),
                         [this](WebSocketConnectionPtr connection) {
                             stop(std::move(connection));
                         });
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        BOOST_CHECK(ws->getError());
        try {
            std::rethrow_exception(ws->getError());
        } catch (WebSocketError &e) {
//            std::cerr << e.what() << std::endl;
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testWebSocketNetworkTimeout() {
        unsigned short port = getUnusedPort();
        WebSocketConnect(String::format("ws://localhost:%d/", port),
                         [this](WebSocketConnectionPtr connection) {
                             stop(std::move(connection));
                         }, 0.01f);
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        BOOST_CHECK(ws->getError());
        try {
            std::rethrow_exception(ws->getError());
        } catch (_HTTPError &e) {
            BOOST_CHECK_EQUAL(e.getCode(), 599);
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testWebSocketNetworkFail() {
        unsigned short port = getUnusedPort();
        WebSocketConnect(String::format("ws://localhost:%d/", port),
                         [this](WebSocketConnectionPtr connection) {
                             stop(std::move(connection));
                         }, 3600.0f);
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        BOOST_CHECK(ws->getError());
        try {
            std::rethrow_exception(ws->getError());
        } catch (_HTTPError &e) {
            BOOST_CHECK_EQUAL(e.getCode(), 599);
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testWebSocketCloseBufferedData() {
        WebSocketConnect(String::format("ws://localhost:%d/echo", getHTTPPort()),
                         [this](WebSocketConnectionPtr connection) {
//                             std::cerr << "On Connect" << std::endl;
                             stop(std::move(connection));
                         });
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        ws->writeMessage("hello");
        ws->writeMessage("world");
        ws->getStream()->close();
        std::string closed = wait<std::string>();
        BOOST_CHECK_EQUAL(closed, "closed");
//        std::cerr << "Success" << std::endl;
    }

    void testWebSocketHeaders() {
        HTTPHeaders headers{{"X-Test", "hello"}};
        auto request = HTTPRequest::create(String::format("ws://localhost:%d/header", getHTTPPort()),
                                           ARG_headers=headers);
        WebSocketConnect(std::move(request),
                         [this](WebSocketConnectionPtr connection) {
//                             std::cerr << "On Connect" << std::endl;
                             stop(std::move(connection));
                         });
        WebSocketConnectionPtr ws = wait<WebSocketConnectionPtr>();
        ws->readMessage([this](boost::optional<ByteArray> data) {
//            std::cerr << "On Read" << std::endl;
            BOOST_REQUIRE_NE(data.get_ptr(), static_cast<const ByteArray *>(nullptr));
            stop(std::move(data.get()));
        });
        ByteArray response = wait<ByteArray>();
        BOOST_CHECK_EQUAL(String::toString(response), "hello");
        ws->getStream()->close();
        std::string closed = wait<std::string>();
        BOOST_CHECK_EQUAL(closed, "closed");
    }
};


void TestWebSocketHandler::onClose() {
    _test->stop(std::string{"closed"});
}


class MaskFunctionTest: public TestCase {
public:
    void testMask() {
        do {
            std::array<Byte, 4> mask = {'a', 'b', 'c', 'd'};
            ByteArray data = {};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);

        do {
            std::array<Byte, 4> mask = {'a', 'b', 'c', 'd'};
            ByteArray data = {'b'};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {'\x03'};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);

        do {
            std::array<Byte, 4> mask = {'a', 'b', 'c', 'd'};
            ByteArray data = {'5', '4', '3', '2', '1'};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {'T', 'V', 'P', 'V', 'P'};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);

        do {
            std::array<Byte, 4> mask = {'Z', 'X', 'C', 'V'};
            ByteArray data = {'9', '8', '7', '6', '5', '4', '3', '2'};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {'c', '`', 't', '`', 'o', 'l', 'p', 'd'};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);

        do {
            std::array<Byte, 4> mask = {'\x00', '\x01', '\x02', '\x03'};
            ByteArray data = {0xff, 0xfb, 0xfd, 0xfc, 0xfe, 0xfa};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {0xff, 0xfa, 0xff, 0xff, 0xfe, 0xfb};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);

        do {
            std::array<Byte, 4> mask = {0xff, 0xfb, 0xfd, 0xfc};
            ByteArray data = {'\x00', '\x01', '\x02', '\x03', '\x04', '\x05'};
            WebSocketMask(mask, data.data(), data.size());
            ByteArray rhs = {0xff, 0xfa, 0xff, 0xff, 0xfb, 0xfe};
            BOOST_CHECK_EQUAL(data, rhs);
        } while (false);
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCallbacks)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPFail)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPSuccess)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketNetworkTimeout)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketNetworkFail)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCloseBufferedData)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHeaders)
TINYCORE_TEST_CASE(MaskFunctionTest, testMask)
