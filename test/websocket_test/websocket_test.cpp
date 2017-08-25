//
// Created by yuwenyong on 17-8-18.
//

#define BOOST_TEST_MODULE websocket_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class EchoHandler: public WebSocketHandler {
public:
    using WebSocketHandler::WebSocketHandler;

//    void onOpen(const StringVector &args) override {
//        std::cerr << "On Open" << std::endl;
//    }

    void onMessage(ByteArray data) override {
//        std::cerr << "On Message" << std::endl;
        writeMessage(data);
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
        Application::HandlersType handlers = {
                url<EchoHandler>("/echo"),
                url<NonWebSocketHandler>("/non_ws"),
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

    void testWebSocketNetworkFail() {
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
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCallbacks)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPFail)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPSuccess)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketNetworkFail)
