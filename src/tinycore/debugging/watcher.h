//
// Created by yuwenyong on 17-4-5.
//

#ifndef TINYCORE_WATCHER_H
#define TINYCORE_WATCHER_H

#include "tinycore/common/common.h"
#include <mutex>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/signals2.hpp>
#include "tinycore/logging/log.h"
#include "tinycore/utilities/string.h"


class Watcher {
public:
    typedef std::map<std::string, int> ObjectMapType;
    typedef std::function<bool (const std::string &, int)> FilterType;
    typedef std::function<void (int, int)> SetCallbackType;
    typedef std::function<void (int, int, int)> IncCallbackType;
    typedef std::function<void (int, int, int)> DecCallbackType;
    typedef std::function<void (int)> DelCallbackType;
    typedef boost::signals2::signal<void (int, int)> SetCallbackSignalType;
    typedef boost::signals2::signal<void (int, int, int)> IncCallbackSignalType;
    typedef boost::signals2::signal<void (int, int, int)> DecCallbackSignalType;
    typedef boost::signals2::signal<void (int)> DelCallbackSignalType;
    typedef boost::ptr_map<std::string, SetCallbackSignalType> SetCallbackContainerType;
    typedef boost::ptr_map<std::string, IncCallbackSignalType> IncCallbackContainerType;
    typedef boost::ptr_map<std::string, DecCallbackSignalType> DecCallbackContainerType;
    typedef boost::ptr_map<std::string, DelCallbackSignalType> DelCallbackContainerType;

    Watcher() = default;
    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;
    ~Watcher() = default;
    void set(const char *key, int value);
    void inc(const char *key, int increment=1);
    void dec(const char *key, int decrement=1);
    void del(const char *key);
    void addSetCallback(const char *key, SetCallbackType callback);
    void addIncCallback(const char *key, IncCallbackType callback);
    void addDecCallback(const char *key, DecCallbackType callback);
    void addDelCallback(const char *key, DelCallbackType callback);
    void dump(FilterType filter, Logger *logger=nullptr) const;
    void dumpAll(Logger *logger=nullptr) const;
    void dumpNonZero(Logger *logger=nullptr) const;

    static Watcher* instance();
protected:
    void dumpHeader(Logger *logger) const {
        const char *border = "+----------------------------------------|--------------------+";
        if (logger) {
            logger->info(border);
            logger->info("|%-40s|%-20s|", "ObjectKey", "CurrentValue");
        } else {
            Log::info(border);
            Log::info("|%-40s|%-20s|", "ObjectKey", "CurrentValue");
        }
    }

    void dumpObject(const std::string &key, int value, Logger *logger) const {
        const char *border = "+----------------------------------------|--------------------+";
        if (logger) {
            logger->info(border);
            logger->info("|%-40s|%-20d|", key.c_str(), value);
        } else {
            Log::info(border);
            Log::info("|%-40s|%-20d|", key.c_str(), value);
        }
    }

    void dumpFooter(Logger *logger) const {
        const char *border = "+----------------------------------------|--------------------+";
        if (logger) {
            logger->info(border);
        } else {
            Log::info(border);
        }
    }

    mutable std::mutex _lock;
    ObjectMapType _objs;
    SetCallbackContainerType _setCallbacks;
    IncCallbackContainerType _incCallbacks;
    DecCallbackContainerType _decCallbacks;
    DelCallbackContainerType _delCallbacks;
};

#define sWatcher Watcher::instance()

#endif //TINYCORE_WATCHER_H
