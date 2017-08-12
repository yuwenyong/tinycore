//
// Created by yuwenyong on 17-8-12.
//

#include "tinycore/configuration/globalinit.h"
#include "tinycore/asyncio/logutil.h"
#include "tinycore/configuration/options.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/logging.h"
#include "tinycore/utilities/objectmanager.h"


GlobalInit::_inited = false;

void GlobalInit::initFromEnvironment() {
    assert(!_inited);
    LogUtil::initGlobalLoggers();
    LogUtil::enablePrettyLogging(sOptions);
    sOptions->praseEnvironment([](std::string name){
        return name;
    });
    if (!Logging::isInitialized()) {
        Logging::init();
    }
    setupWatcherHook();
    _inited = true;
}

void GlobalInit::initFromCommandLine(int argc, const char *const *argv) {
    assert(!_inited);
    LogUtil::initGlobalLoggers();
    LogUtil::defineLoggingOptions(sOptions);
    sOptions->parseCommandLine(argc, argv);
    if (!Logging::isInitialized()) {
        Logging::init();
    }
    setupWatcherHook();
    _inited = true;
}

void GlobalInit::initFromConfigFile(const char *path) {
    assert(!_inited);
    LogUtil::initGlobalLoggers();
    LogUtil::defineLoggingOptions(sOptions);
    sOptions->parseConfigFile(path);
    if (!Logging::isInitialized()) {
        Logging::init();
    }
    setupWatcherHook();
    _inited = true;
}

void GlobalInit::cleanup() {
    sObjectMgr->cleanup();
    sWatcher->dumpAll();
    Logging::close();
}

void GlobalInit::setupWatcherHook() {
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