//
// Created by yuwenyong on 17-7-11.
//

#include "tinycore/tinycore.h"


class HelloWorld: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) {
        finish("Hello World!");
    }
};


class ChatUser: public WebSocketHandler {
public:
    using WebSocketHandler::WebSocketHandler;
    typedef std::shared_ptr<ChatUser> SelfType;

    void onOpen(const StringVector &args) override {
        _userName = getArgument("name", "anonymous");
        std::string message = String::format("User (%s) enter chat room", _userName.c_str());
        for (auto &u: _users) {
            u->writeMessage(message);
        }
        _users.insert(getSelf<ChatUser>());
    }

    void onMessage(ByteArray data) override {
        data.push_back(0x00);
        std::string message = String::format("User (%s) says:%s", _userName.c_str(), (const char *)data.data());
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

int main(int argc, char **argv) {
    ParseCommandLine(argc, argv);
    Application application({
                                    url<HelloWorld>("/"),
                                    url<ChatUser>("/chat"),
                            });
    application.listen(3080, "::");
    sIOLoop->start();
    return 0;
}