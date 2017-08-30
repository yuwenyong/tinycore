//
// Created by yuwenyong on 17-8-18.
//

#define BOOST_TEST_MODULE websocket_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class WebSocketTest;

class EchoHandler: public WebSocketHandler {
public:
    using WebSocketHandler::WebSocketHandler;

    void initialize(ArgsType &args) override {
        _test = boost::any_cast<WebSocketTest *>(args["test"]);
    }

//    void onOpen(const StringVector &args) override {
//        std::cerr << "On Open" << std::endl;
//    }

    void onMessage(ByteArray data) override {
//        std::cerr << "On Message" << std::endl;
        writeMessage(data);
    }

    void onClose() override;
protected:
    WebSocketTest *_test{nullptr};
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
};


void EchoHandler::onClose() {
    _test->stop(std::string{"closed"});
}


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCallbacks)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPFail)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketHTTPSuccess)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketNetworkFail)
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCloseBufferedData)
