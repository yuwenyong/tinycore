//
// Created by yuwenyong on 17-3-7.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/configmgr.h"
#include <thread>


int main() {
    Log::initialize();
    std::cout << "Begin" << std::endl;
    Log::debug("debug msg %s !", "hehe");
    Log::info("info msg %s !", "hehe");
    Log::warn("warn msg %s !", "hehe");
//    std::this_thread::sleep_for(std::chrono::seconds(10));
    Log::error("Error msg %s !", "hehe");
    Log::fatal("Fatal msg %s !", "hehe");
    for (int i = 0; i < 1000; ++i) {
        Log::info("Test msg %d", i);
    }
    std::cout << "End" << std::endl;
    return 0;
}