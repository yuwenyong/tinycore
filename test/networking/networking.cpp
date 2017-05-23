//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/options.h"
#include "tinycore/asyncio/ioloop.h"
#include <regex>
#include <boost/assign.hpp>
#include "tinycore/asyncio/httpserver.h"
#include "tinycore/asyncio/web.h"
#include "tinycore/crypto/hashlib.h"
#include "tinycore/asyncio/websocket.h"
#include "tinycore/asyncio/httpclient.h"
//#include <boost/dll.hpp>


using namespace boost::assign;


class Book: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) {
        std::string price = getArgument("price");
        write(String::format("Book name:%s, price:%s\n", args[0].c_str(), price.c_str()));
    }

    void onPost(StringVector args) {
        write("Create book " + args[0]);
    }

    void onPut(StringVector args) {
        write("Update book " + args[0]);
    }

    void onDelete(StringVector args) {
        write("Delete book " + args[0]);
    }
};


class ChatUser: public WebSocketHandler {
public:
    using WebSocketHandler::WebSocketHandler;
    typedef std::shared_ptr<ChatUser> SelfType;

    void onOpen(const StringVector &args) override {
        std::cout << "OnOpen" << std::endl;
//        setCookie("name", "value");
        _userName = getArgument("name", "anonymous");
        std::string message = String::format("User (%s) enter chat room", _userName.c_str());
        for (auto &u: _users) {
            u->writeMessage(message);
        }
        _users.insert(getSelf<ChatUser>());
    }

    void onMessage(const char *data, size_t length) override {
        std::string message(data, length);
        message = String::format("User (%s) says:%s", _userName.c_str(), message.c_str());
        auto self = getSelf<ChatUser>();
        for (auto &u: _users) {
            if (u != self) {
                u->writeMessage(message);
            }
        }
    }

    void onClose() override {
        _users.erase(getSelf<ChatUser>());
        std::string message = String::format("User (%s) leave chat room", _userName.c_str());
        for (auto &u: _users) {
            u->writeMessage(message);
        }
    }

protected:
    std::string _userName;
    static std::set<SelfType> _users;
};

std::set<ChatUser::SelfType> ChatUser::_users;


class HelloWorld: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) {
//        Asynchronous()
//        auto client = HTTPClient::create();
//        StringMap formArgs = {{"name", "not sb"}, };
//        std::string formString = URLParse::urlEncode(formArgs);
//        ByteArray body(formString.data(), formString.data() + formString.size());
//        HTTPHeaders headers;
//        auto request = HTTPRequest::create("http://localhost:3030/", followRedirects_=false, method_="POST",
//                                           body_=body, connectTimeout_=30.0f, headers_=headers);
//        client->fetch("http://www.csdn.net/",
//                      std::bind(&HelloWorld::onResp, getSelf<HelloWorld>(), std::placeholders::_1));
        write("Hello world");
        FATAL(false, "Exit");
    }

    void onResp(const HTTPResponse &response) {
        const ByteArray *body = response.getBody();
        std::string message((const char *)body->data(), body->size());
        Log::info("HTTP code:%d", response.getCode());
        Log::info("Message:%s", message.c_str());
        write("Hello world");
        finish();
    }
};


int main(int argc, char **argv) {
    ParseCommandLine(argc, argv);
    Application application({
                                    url<HelloWorld>("/"),
                                    url<Book>(R"(/books/(\w+)/)"),
                                    url<ChatUser>("/chat"),
                            });
    HTTPServer server(HTTPServerCB(application));
    server.listen(3080);
    sIOLoop->start();
    return 0;
}
