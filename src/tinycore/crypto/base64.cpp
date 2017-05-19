//
// Created by yuwenyong on 17-4-28.
//

#include "tinycore/crypto/base64.h"
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>


using namespace boost::archive::iterators;

std::string Base64::b64encode(const std::string &s, const char *altChars) {
    typedef base64_from_binary<transform_width<std::string::const_iterator, 6, 8>> Base64EncodeIterator;
    std::string result;
    std::copy(Base64EncodeIterator(s.begin()), Base64EncodeIterator(s.end()), std::back_inserter(result));
    size_t extra = result.length() % 3;
    if (extra) {
        extra = 3 - extra;
        for (size_t i = 0; i != extra; ++i) {
            result.push_back('=');
        }
    }
    if (altChars) {
        for (auto &c: result) {
            if (c == '+') {
                c = altChars[0];
            } else if (c == '/') {
                c = altChars[1];
            }
        }
    }
    return result;
}

std::string Base64::b64decode(std::string s, const char *altChars) {
    typedef transform_width<binary_from_base64<std::string::const_iterator>, 8, 6> Base64DecodeIterator;
    std::string result;
    if (altChars) {
        for (auto &c: s) {
            if (c == altChars[0]) {
                c = '+';
            } else if (c == altChars[1]) {
                c = '/';
            }
        }
    }
    size_t extra = s.length() % 3;
    if (extra) {
        extra = 3 - extra;
        for (size_t i = 0; i != extra; ++i) {
            s.push_back('=');
        }
    }
    std::copy(Base64DecodeIterator(s.begin()), Base64DecodeIterator(s.end()), std::back_inserter(result));
    return result;
}