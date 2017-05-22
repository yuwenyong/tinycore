//
// Created by yuwenyong on 17-4-14.
//

#include "tinycore/configuration/options.h"
#include <iostream>
#include "tinycore/configuration/configmgr.h"
#include "tinycore/logging/log.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/utilities/objectmanager.h"


Options::Options()
        : _opts("Allowed options") {
    define("version,v", "print version string");
    define("help,h", "display help message");
    define<std::string>("config,c", "specify config file");
}

void Options::parseCommandLine(int argc, const char * const argv[]) {
    po::store(po::parse_command_line(argc, argv, _opts), _vm);
    po::notify(_vm);
    if (contain("help")) {
        std::cout << _opts << std::endl;
        exit(1);
    }
    if (contain("version")) {
        std::cout << TINYCORE_VER << std::endl;
        exit(2);
    }
    if (contain("config")) {
        sConfigMgr->loadInitial(get<std::string>("config"));
    }
    onEnter();
}

void Options::onEnter() {
    setupWatcherHook();
    Log::initialize();
}

void Options::onExit() {
    sObjectMgr->finish();
    sWatcher->dumpAll();
    Log::close();
}

Options* Options::instance() {
    static Options instance;
    return &instance;
}

void Options::setupWatcherHook() {
#ifndef NDEBUG
    sWatcher->addIncCallback(SYS_TIMEOUT_COUNT, [](int oldValue, int increment, int value) {
        Log::debug("Create Timeout,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_TIMEOUT_COUNT, [](int oldValue, int decrement, int value) {
        Log::debug("Destroy Timeout,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_IOSTREAM_COUNT, [](int oldValue, int increment, int value) {
        Log::debug("Create IOStream,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_IOSTREAM_COUNT, [](int oldValue, int decrement, int value) {
        Log::debug("Destroy IOStream,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_SSLIOSTREAM_COUNT, [](int oldValue, int increment, int value) {
        Log::debug("Create SSLIOStream,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_SSLIOSTREAM_COUNT, [](int oldValue, int decrement, int value) {
        Log::debug("Destroy SSLIOStream,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPCONNECTION_COUNT, [](int oldValue, int increment, int value) {
        Log::debug("Create HTTPConnection,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPCONNECTION_COUNT, [](int oldValue, int decrement, int value) {
        Log::debug("Destroy HTTPConnection,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPSERVERREQUEST_COUNT, [](int oldValue, int increment, int value) {
        Log::debug("Create HTTPServerRequest,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPSERVERREQUEST_COUNT, [](int oldValue, int decrement, int value) {
        Log::debug("Destroy HTTPServerRequest,current count:%d", value);
    });
#endif
}