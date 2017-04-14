//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/networking/ioloop.h"
#include <regex>
#include <tinycore/networking/httpserver.h>


int main() {
    Log::initialize();
    IOLoop ioloop;
    Timeout timeout = ioloop.addTimeout(10.0, [&ioloop](){
        Log::info("First Timeout");
//        ioloop.stop();
    });
//    Timeout timeout2 = ioloop.addTimeout(5.0, [](){
//        Log::info("Second Timeout");
//    });
    auto repeatTimer = PeriodicCallback::create([]() {
        Log::info("repeat Timer");
    }, 1.0f, &ioloop);
    repeatTimer->start();
//    IOLoop::instance()->removeTimeout(timeout2);
//    IOLoop::instance()->removeTimeout(timeout2);
    HTTPServer server([](HTTPRequestPtr request){
        Log::info("Hello world");
        request->write("Hello world!!! Dont ignore\r\n");
        request->finish();
    }, false, &ioloop);
    server.listen(7070);
    Log::info("Server start");
    ioloop.start();
    Log::info("Server end");
    return 0;
}
