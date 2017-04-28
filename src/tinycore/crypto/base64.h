//
// Created by yuwenyong on 17-4-28.
//

#ifndef TINYCORE_BASE64_H
#define TINYCORE_BASE64_H

#include "tinycore/common/common.h"


class Base64 {
public:
    static std::string b64encode(const std::string &s, const char *altChars= nullptr);
    static std::string b64decode(std::string s, const char *altChars= nullptr);
};

#endif //TINYCORE_BASE64_H
