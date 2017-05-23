//
// Created by yuwenyong on 17-4-1.
//

#include "tinycore/utilities/objectmanager.h"


ObjectManager::~ObjectManager() {
    cleanup();
}


ObjectManager* ObjectManager::instance() {
    static ObjectManager instance;
    return &instance;
}