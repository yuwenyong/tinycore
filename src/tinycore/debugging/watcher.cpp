//
// Created by yuwenyong on 17-4-5.
//

#include "tinycore/debugging/watcher.h"


void Watcher::set(const char *key, int value) {
    std::lock_guard<std::mutex> lock(_objsLock);
    auto iter = _objs.find(key);
    if (iter == _objs.end()) {
        _objs.emplace(key, value);
    } else {
        iter->second = value;
    }
}

void Watcher::inc(const char *key, int increment) {
    std::lock_guard<std::mutex> lock(_objsLock);
    auto iter = _objs.find(key);
    if (iter == _objs.end()) {
        _objs.emplace(key, increment);
    } else {
        iter->second += increment;
    }
}

void Watcher::dec(const char *key, int decrement) {
    std::lock_guard<std::mutex> lock(_objsLock);
    auto iter = _objs.find(key);
    if (iter == _objs.end()) {
        _objs.emplace(key, -decrement);
    } else {
        iter->second -= decrement;
    }
}

void Watcher::del(const char *key) {
    std::lock_guard<std::mutex> lock(_objsLock);
    _objs.erase(key);
}

void Watcher::dump(FilterType filter, Logger *logger) const {
    std::lock_guard<std::mutex> lock(_objsLock);
    dumpHeader(logger);
    for (const auto &object: _objs) {
        if (filter(object.first, object.second)) {
            dumpObject(object.first,  object.second, logger);
        }
    }
    dumpFooter(logger);
}

void Watcher::dumpAll(Logger *logger) const {
    std::lock_guard<std::mutex> lock(_objsLock);
    dumpHeader(logger);
    for (const auto &object: _objs) {
        dumpObject(object.first,  object.second, logger);
    }
    dumpFooter(logger);
}

void Watcher::dumpNonZero(Logger *logger) const {
    std::lock_guard<std::mutex> lock(_objsLock);
    dumpHeader(logger);
    for (const auto &object: _objs) {
        if (object.second) {
            dumpObject(object.first,  object.second, logger);
        }
    }
    dumpFooter(logger);
}

Watcher* Watcher::instance() {
    static Watcher instance;
    return &instance;
}