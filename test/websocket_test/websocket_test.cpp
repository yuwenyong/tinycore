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


class WebSocketTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>("/echo"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testWebSocketCallbacks() {
        using WebSocketConnectionPtr = std::shared_ptr<WebSocketClientConnection>;
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
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(WebSocketTest, testWebSocketCallbacks)

