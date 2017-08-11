//
// Created by yuwenyong on 17-4-14.
//

#include "tinycore/configuration/options.h"
#include <iostream>
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/logging.h"
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
//        sConfigMgr->loadInitial(get<std::string>("config"));
    }
    onEnter();
}

void Options::onEnter() {
    Logging::init();
    setupWatcherHook();
    setupInterrupter();
}

void Options::onExit() {
    sObjectMgr->cleanup();
    sWatcher->dumpAll();
    Logging::close();
}

Options* Options::instance() {
    static Options instance;
    return &instance;
}

void Options::setupWatcherHook() {
#ifndef NDEBUG
    sWatcher->addIncCallback(SYS_TIMEOUT_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create Timeout,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_TIMEOUT_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy Timeout,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_PERIODICCALLBACK_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create PeriodicCallback,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_PERIODICCALLBACK_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy PeriodicCallback,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_IOSTREAM_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create IOStream,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_IOSTREAM_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy IOStream,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_SSLIOSTREAM_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create SSLIOStream,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_SSLIOSTREAM_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy SSLIOStream,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPCONNECTION_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create HTTPConnection,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPCONNECTION_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy HTTPConnection,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPSERVERREQUEST_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create HTTPServerRequest,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPSERVERREQUEST_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy HTTPServerRequest,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_REQUESTHANDLER_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create RequestHandler,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_REQUESTHANDLER_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy RequestHandler,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_WEBSOCKETHANDLER_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create WebsocketHandler,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_WEBSOCKETHANDLER_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy WebsocketHandler,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPCLIENT_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create HTTPClient,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPCLIENT_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy HTTPClient,current count:%d", value);
    });

    sWatcher->addIncCallback(SYS_HTTPCLIENTCONNECTION_COUNT, [](int oldValue, int increment, int value) {
        LOG_TRACE("Create HTTPClientConnection,current count:%d", value);
    });
    sWatcher->addDecCallback(SYS_HTTPCLIENTCONNECTION_COUNT, [](int oldValue, int decrement, int value) {
        LOG_TRACE("Destroy HTTPClientConnection,current count:%d", value);
    });
#endif
}

void Options::setupInterrupter() {
//    if (!sConfigMgr->getBool("disableInterrupter", false)) {
//        sIOLoop->signal(SIGINT, [this](){
//            Log::info("Capture SIGINT");
//            sIOLoop->stop();
//            return -1;
//        });
//        sIOLoop->signal(SIGTERM, [this](){
//            Log::info("Capture SIGTERM");
//            sIOLoop->stop();
//            return -1;
//        });
//#if defined(SIGQUIT)
//        sIOLoop->signal(SIGQUIT, [this](){
//            Log::info("Capture SIGQUIT");
//            sIOLoop->stop();
//            return -1;
//        });
//#endif
//    }
}