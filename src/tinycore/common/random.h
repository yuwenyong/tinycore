//
// Created by yuwenyong on 17-8-16.
//

#ifndef TINYCORE_RANDOM_H
#define TINYCORE_RANDOM_H

#include "tinycore/common/common.h"
#include <random>


class Random {
public:
    static void randBytes(Byte *buffer, size_t length) {
        std::random_device rd;
        std::uniform_int_distribution<Byte> dist;
        for (size_t i = 0; i != length; ++i) {
            buffer[i] = dist(rd);
        }
    }

    template <size_t BufLen>
    static void randBytes(std::array<Byte, BufLen> &buffer) {
        std::random_device rd;
        std::uniform_int_distribution<Byte> dist;
        for (size_t i = 0; i != BufLen; ++i) {
            buffer[i] = dist(rd);
        }
    }
protected:
    static std::default_random_engine _engine;
};


#endif //TINYCORE_RANDOM_H
