//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/logging/log.h"
#include "tinycore/networking/ioloop.h"


int main() {
    Log::initialize();
    TimeoutPtr timeout = IOLoop::instance()->addTimeout(5.0, [](){
        Log::info("Timeout");
    });
    TimeoutPtr timeout2 = IOLoop::instance()->addTimeout(10.0, [](){
        Log::info("Second Timeout");
    });
    IOLoop::instance()->removeTimeout(timeout2);
    IOLoop::instance()->removeTimeout(timeout2);
    Log::info("Server start");
    IOLoop::instance()->start();
    Log::info("Server end");
    return 0;
}