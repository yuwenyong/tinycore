//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/options.h"
#include "tinycore/networking/ioloop.h"
#include <regex>
#include "tinycore/networking/httpserver.h"
#include "tinycore/networking/web.h"
//#include <boost/dll.hpp>


int main(int argc, char **argv) {
//    boost::dll::this_line_location();
    ParseCommandLine(argc, argv);
    Application app;
    Timeout timeout = sIOLoop->addTimeout(10.0, [](){
        Log::info("First Timeout");
//        ioloop.stop();
    });
//    Timeout timeout2 = ioloop.addTimeout(5.0, [](){
//        Log::info("Second Timeout");
//    });
    auto repeatTimer = PeriodicCallback::create([]() {
        Log::info("repeat Timer");
    }, 1.0f);
    repeatTimer->start();
//    IOLoop::instance()->removeTimeout(timeout2);
//    IOLoop::instance()->removeTimeout(timeout2);
    HTTPServer server([](HTTPRequestPtr request){
        Log::info("Hello world");
        request->write("Hello world!!! Dont ignore\r\n");
        request->finish();
    }, false);
    server.listen(7070);
    Log::info("Server start");
    sIOLoop->start();
    Log::info("Server end");
    return 0;
}
