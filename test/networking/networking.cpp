//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/options.h"
#include "tinycore/networking/ioloop.h"
#include <regex>
#include <boost/assign.hpp>
#include "tinycore/networking/httpserver.h"
#include "tinycore/networking/web.h"
#include "tinycore/crypto/hashlib.h"
//#include <boost/dll.hpp>


using namespace boost::assign;

class HelloWorld: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) {
        write("Hello world");
    }
};


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


int main(int argc, char **argv) {
    ParseCommandLine(argc, argv);
    Application application({
                                    url<HelloWorld>("/"),
                                    url<Book>(R"(/books/(\w+)/)"),
                            });
    HTTPServer server(HTTPServerCB(application));
    server.listen(3080);
    sIOLoop->start();
    return 0;
}
