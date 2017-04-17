//
// Created by yuwenyong on 17-4-1.
//

#ifndef TINYCORE_OBJECTMANAGER_H
#define TINYCORE_OBJECTMANAGER_H

#include "tinycore/common/common.h"
#include <mutex>
#include <boost/foreach.hpp>
#include <boost/functional/factory.hpp>
#include <boost/checked_delete.hpp>


class CleanupObject {
public:
    virtual ~CleanupObject() {};
    virtual void cleanup() = 0;
};


class ObjectManager {
public:
    typedef std::list<CleanupObject*> CleanupObjectContainer;

    ObjectManager() {

    }

    ~ObjectManager();

    template <typename T>
    T* registerObject() {
        std::lock_guard<std::mutex> lock(_objectsLock);
        if (finished()) {
            return nullptr;
        }
        T *object = boost::factory<T *>()();
        _cleanupObjects.push_back(object);
        return object;
    }

    void finish() {
        std::lock_guard<std::mutex> lock(_objectsLock);
        if (finished()) {
            return;
        }
        BOOST_REVERSE_FOREACH(CleanupObject *object, _cleanupObjects) { object->cleanup(); }
        _cleanupObjects.clear();
        _finished = true;
    }

    bool finished() const {
        return _finished;
    }

    static ObjectManager* instance();
protected:
    bool _finished{false};
    CleanupObjectContainer _cleanupObjects;
    std::mutex _objectsLock;
};


#define sObjectMgr ObjectManager::instance()

template <typename T>
class Singleton: public CleanupObject {
public:
//    friend class ObjectManager;

    void cleanup() override {
        boost::checked_delete(this);
        _singleton = nullptr;
    }

    static T* instance() {
        if (!_singleton) {
            _singleton = sObjectMgr->registerObject<Singleton<T>>();
        }
        return _singleton ? &(_singleton->_instance) : nullptr;
    }

protected:
//    Singleton() = default;
    T _instance;
    static Singleton<T> *_singleton;
};

template <typename T>
Singleton<T>* Singleton<T>::_singleton = nullptr;


#endif //TINYCORE_OBJECTMANAGER_H
