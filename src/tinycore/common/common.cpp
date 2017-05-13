//
// Created by yuwenyong on 17-2-4.
//

#include "tinycore/common/common.h"

const char* StrNStr(const char *s1,size_t len1, const char *s2, size_t len2) {
    if (len2 > len1)
        return nullptr;
    const size_t len = len1 - len2;
    for (size_t i = 0; i <= len; i++) {
        if (memcmp (s1 + i, s2, len2) == 0) {
            return s1 + i;
        }
    }
    return nullptr;
}