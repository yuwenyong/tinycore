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

    void registerObject(CleanupObject *object) {
        std::lock_guard<std::mutex> lock(_objectsLock);
        _cleanupObjects.push_back(object);
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(_objectsLock);
        BOOST_REVERSE_FOREACH(CleanupObject *object, _cleanupObjects) { object->cleanup(); }
        _cleanupObjects.clear();
    }

    static ObjectManager* instance();
protected:
    CleanupObjectContainer _cleanupObjects;
    std::mutex _objectsLock;
};


#define sObjectMgr ObjectManager::instance()


template <typename T>
class Singleton: public CleanupObject {
public:
    void cleanup() override {
        boost::checked_delete(this);
        _singleton = nullptr;
    }

    static T* instance() {
        return _singleton ? &(_singleton->instance) : nullptr;
    }

    static T* createInstance() {
        if (_singleton == nullptr) {
            _singleton = boost::factory<Singleton<T> *>()();
            sObjectMgr->registerObject(_singleton);
        }
        return &(_singleton->_instance);
    }
protected:
    Singleton() = default;
    T _instance;
    static Singleton<T> *_singleton;
};

template <typename T>
Singleton<T>* Singleton<T>::_singleton = nullptr;

#endif //TINYCORE_OBJECTMANAGER_H
