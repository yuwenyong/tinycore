//
// Created by yuwenyong on 17-4-5.
//

#ifndef TINYCORE_WATCHER_H
#define TINYCORE_WATCHER_H

#include "tinycore/common/common.h"
#include <mutex>
#include "tinycore/logging/log.h"
#include "tinycore/utilities/string.h"

class Watcher {
public:
    typedef std::map<std::string, int> ObjectMap;
    typedef std::function<bool (const std::string &, int)> FilterType;

    Watcher() = default;
    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;
    ~Watcher() = default;
    void set(const char *key, int value);
    void inc(const char *key, int increment=1);
    void dec(const char *key, int decrement=1);
    void del(const char *key);
    void dump(FilterType filter, Logger *logger=nullptr) const;
    void dumpAll(Logger *logger=nullptr) const;
    void dumpNonZero(Logger *logger=nullptr) const;

    static Watcher* instance();
protected:
    void dumpHeader(Logger *logger) const {
        const char *content = "-----------------------------------Dump begin-----------------------------------";
        if (logger) {
            logger->info(content);
        } else {
            Log::info(content);
        }
    }

    void dumpObject(const std::string &key, int value, Logger *logger) const {
        if (logger) {
            logger->info("%s:%d", key.c_str(), value);
        } else {
            Log::info("%s:%d", key.c_str(), value);
        }
    }

    void dumpFooter(Logger *logger) const {
        const char *content = "-----------------------------------Dump end-------------------------------------";
        if (logger) {
            logger->info(content);
        } else {
            Log::info(content);
        }
    }

    ObjectMap _objs;
    mutable std::mutex _objsLock;
};

#define sWatcher Watcher::instance()

#endif //TINYCORE_WATCHER_H
