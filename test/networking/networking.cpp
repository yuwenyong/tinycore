//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/networking/ioloop.h"


int main() {
    Log::initialize();
    IOLoop ioloop;
    TimeoutPtr timeout = ioloop.addTimeout(5.0, [&ioloop](){
        Log::info("First Timeout");
        ioloop.stop();
    });
    TimeoutPtr timeout2 = ioloop.addTimeout(10.0, [](){
        Log::info("Second Timeout");
    });
//    IOLoop::instance()->removeTimeout(timeout2);
//    IOLoop::instance()->removeTimeout(timeout2);
    Log::info("Server start");
    ioloop.start();
    Log::info("Server end");
    return 0;
}