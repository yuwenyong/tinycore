//
// Created by yuwenyong on 17-3-7.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/configmgr.h"


int main() {
    Log::initialize();
    Log::debug("debug msg %s !", "hehe");
    Log::info("info msg %s !", "hehe");
    Log::warn("warn msg %s !", "hehe");
    Log::error("Error msg %s !", "hehe");
    Log::fatal("Fatal msg %s !", "hehe");
    return 0;
}