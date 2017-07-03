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

    void onMessage(ByteArray data) override {
        std::string message((const char *)data.data(), data.size());
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
        Asynchronous()
        Log::info("onGet");
        HTTPClient::create()->fetch("http://www.csdn.net/", std::bind(&HelloWorld::onStep1, this,
                                                                      std::placeholders::_1));
    }

    void onStep1(HTTPResponse response) {
        Log::info("onStep1");
        HTTPClient::create()->fetch("http://www.csdn.net/", std::bind(&HelloWorld::onStep2, this,
                                                                      std::placeholders::_1));
    }

    void onStep2(HTTPResponse response) {
        Log::info("onStep2");
        auto name = getArgument("name");
        finish(String::format("Your name is %s", name.c_str()));
    }
};


template <typename... Args>
class WP {
public:
    WP(std::function<int (Args...)> foo): _foo(std::move(foo)) {}

    int operator()(Args&&... args) {
        return _foo(std::forward<Args>(args)...);
    }

protected:
    std::function<int (Args...)> _foo;
};


int main(int argc, char **argv) {
//    sConfigMgr->setString("Appender.Console", "Console,1,7");
    ParseCommandLine(argc, argv);

//    std::function<int (int, int)> foo = [](int a, int b) {
//        return a + b;
//    };
//    std::cerr << foo.target_type().name() << std::endl;
//    if (foo.target_type() == typeid(WP<>)) {
//        std::cerr << "Cast success" << std::endl;
//    } else {
//        std::cerr << "Cast fail" << std::endl;
//    }
//    foo = WP<int, int>(std::move(foo));
//    std::cerr << foo.target_type().name() << std::endl;
//    if (foo.target_type() == typeid(WP<int, int>)) {
//        std::cerr << "Cast success" << std::endl;
//    } else {
//        std::cerr << "Cast fail" << std::endl;
//    }
//
//    std::function<int (int, int)> bar = foo;
//    if (bar.target_type() == typeid(WP<int, int>)) {
//        std::cerr << "Cast success" << std::endl;
//    } else {
//        std::cerr << "Cast fail" << std::endl;
//    }

    Application application({
                                    url<HelloWorld>("/"),
                            });
    HTTPServer server(HTTPServerCB(application));
    server.listen(3080);
    sIOLoop->start();
    Log::info("After stop");
    return 0;
}
