//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/tinycore.h"
#include <regex>
#include <boost/assign.hpp>

//#include <boost/dll.hpp>


using namespace boost::assign;


class Book: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) {
        for (auto &s: args) {
            std::cout << s <<std::endl;
        }
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

    void onMessage(const Byte *data, size_t length) override {
        std::string message((const char *)data, length);
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
//        StringMap formArgs = {{"name", "not sb"}, };
//        std::string formString = URLParse::urlEncode(formArgs);
//        ByteArray body(formString.data(), formString.data() + formString.size());
//        HTTPHeaders headers;
//        auto request = HTTPRequest::create("http://localhost:3030/", followRedirects_=false, method_="POST",
//                                           body_=body, connectTimeout_=30.0f, headers_=headers);
//        write("Hello world");
//        FATAL(false, "Exit");
        Asynchronous()
        auto client = HTTPClient::create();
        client->fetch("https://github.com/",
                      std::bind(&HelloWorld::onResp, getSelf<HelloWorld>(), std::placeholders::_1));

    }

    void onResp(const HTTPResponse &response) {
        const ByteArray *body = response.getBody();
        if (body) {
            std::string message((const char *)body->data(), body->size());
            Log::info("HTTP code:%d", response.getCode());
            Log::info("Message:%s", message.c_str());
        } else {
            Log::error("No body found");
        }

        write("Hello world");
        finish();
    }
};


int main(int argc, char **argv) {
//    sConfigMgr->setString("Appender.Console", "Console,1,7");
    ParseCommandLine(argc, argv);
    Application application({
                                    url<HelloWorld>("/"),
                                    url<Book>(R"(/books/(\w+)/)"),
                                    url<ChatUser>("/chat"),
                            }, "", {}, {
            {"gzip", true},
    });
//    HTTPServer server(HTTPServerCB(application));
//    server.listen(3080);
    if (sIOLoop->running()) {
        Log::info("Running");
    } else {
        Log::info("Stopped");
    }
    sIOLoop->addTimeout(1.0f, [](){
        Log::info("First Timer");
        sIOLoop->stop();
    });
    sIOLoop->addTimeout(10.0f, [](){
        Log::info("Second Timer");
    });
    sIOLoop->start();
    Log::info("After stop");
    return 0;
}
