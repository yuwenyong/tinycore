//
// Created by yuwenyong on 17-5-8.
//

#ifndef TINYCORE_CONTAINER_H
#define TINYCORE_CONTAINER_H

#include "tinycore/common/common.h"
#include <boost/ptr_container/ptr_vector.hpp>


template <typename T>
class PtrVector: public boost::ptr_vector<T> {
public:
    typedef boost::ptr_vector<T> SuperType;
    using SuperType::ptr_vector;

    PtrVector(): SuperType() {

    }

    PtrVector(std::initializer_list<T*> il)
            : SuperType() {
        for (auto p: il) {
            SuperType::push_back(p);
        }
    }

    PtrVector(PtrVector &&r)
            : SuperType() {
        SuperType::swap(r);
    }

    PtrVector& operator=(PtrVector &&r) {
        SuperType::clear();
        SuperType::swap(r);
        return *this;
    }
};

#endif //TINYCORE_CONTAINER_H
