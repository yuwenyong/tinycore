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


class MyConnection: public std::enable_shared_from_this<MyConnection> {
public:
    MyConnection(std::shared_ptr<BaseIOStream> stream)
            : _stream(std::move(stream)) {

    }

    void start() {
        _stream->setCloseCallback(std::bind(&MyConnection::onClose, shared_from_this()));
        _stream->readBytes(4, std::bind(&MyConnection::onKBEHeader, shared_from_this(), std::placeholders::_1));
    }

    void onKBEHeader(ByteArray data) {
        uint16 packetId = *(uint16 *)data.data();
        uint16 length = *((uint16 *)data.data() + 1);
        if (length == 0xFFFF) {
            _stream->readBytes(4, std::bind(&MyConnection::onKBELength, shared_from_this(), packetId,
                                            std::placeholders::_1));
        } else {
            _stream->readBytes(length, std::bind(&MyConnection::onKBEPacket, shared_from_this(), packetId,
                                                 std::placeholders::_1));
        }
    }

    void onKBELength(uint16 packetId, ByteArray data) {
        uint32 length = *(uint32 *)data.data();
        _stream->readBytes(length, std::bind(&MyConnection::onKBELength, shared_from_this(), packetId,
                                             std::placeholders::_1));
    }

    void onKBEPacket(uint16 packetId, ByteArray data) {
        // unpack packet,then cached,let logic thread handle it
        _stream->readBytes(4, std::bind(&MyConnection::onKBEHeader, shared_from_this(), std::placeholders::_1));
    }

    void onLength(ByteArray data) {
        Byte length = data[0];
        if (length == 0xFF) {
            _stream->readBytes(4, std::bind(&MyConnection::onLengthExt, shared_from_this(), std::placeholders::_1));
        } else {
            _stream->readBytes(length, std::bind(&MyConnection::onPacket, shared_from_this(), std::placeholders::_1));
        }
    }

    void onLengthExt(ByteArray data) {
        unsigned int length = *(unsigned int *)data.data();
        _stream->readBytes(length, std::bind(&MyConnection::onPacket, shared_from_this(), std::placeholders::_1));
    }

    void onPacket(ByteArray data) {
        // unpack packet,then cached,let logic thread handle it
        _stream->readBytes(1, std::bind(&MyConnection::onLength, shared_from_this(), std::placeholders::_1));
    }

    void onClose() {

    }
protected:
    std::shared_ptr<BaseIOStream> _stream;
};

class MyServer: public TCPServer {
public:
    using TCPServer::TCPServer;

    void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override {
        std::make_shared<MyConnection>(std::move(stream))->start();
    }
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
    auto server = std::make_shared<HTTPServer>(HTTPServerCB(application));
    server->listen(3080);
    sIOLoop->start();
    Log::info("After stop");
    return 0;
}
