//
// Created by yuwenyong on 17-4-28.
//

#include "tinycore/crypto/base64.h"
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>


using namespace boost::archive::iterators;

std::string Base64::b64encode(const std::string &s, const char *altChars) {
    typedef base64_from_binary<transform_width<std::string::const_iterator, 6, 8>> Base64EncodeIterator;
    std::string result;
    std::copy(Base64EncodeIterator(s.begin()), Base64EncodeIterator(s.end()), std::back_inserter(result));
    size_t extra = s.length() % 3;
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

ByteArray Base64::b64encode(const ByteArray &s, const char *altChars) {
    typedef base64_from_binary<transform_width<ByteArray::const_iterator, 6, 8>> Base64EncodeIterator;
    ByteArray result;
    std::copy(Base64EncodeIterator(s.begin()), Base64EncodeIterator(s.end()), std::back_inserter(result));
    size_t extra = s.size() % 3;
    if (extra) {
        extra = 3 - extra;
        for (size_t i = 0; i != extra; ++i) {
            result.push_back('=');
        }
    }
    if (altChars) {
        for (auto &c: result) {
            if (c == '+') {
                c = (Byte)altChars[0];
            } else if (c == '/') {
                c = (Byte)altChars[1];
            }
        }
    }
    return result;
}

std::string Base64::b64decode(const std::string &s, const char *altChars) {
    typedef transform_width<binary_from_base64<std::string::const_iterator>, 8, 6> Base64DecodeIterator;
    std::string temp(s), result;
    if (altChars) {
        for (auto &c: temp) {
            if (c == altChars[0]) {
                c = '+';
            } else if (c == altChars[1]) {
                c = '/';
            }
        }
    }
    std::copy(Base64DecodeIterator(temp.begin()), Base64DecodeIterator(temp.end()), std::back_inserter(result));
    return result;
}

ByteArray Base64::b64decode(const ByteArray &s, const char *altChars) {
    typedef transform_width<binary_from_base64<ByteArray::const_iterator>, 8, 6> Base64DecodeIterator;
    ByteArray temp(s), result;
    if (altChars) {
        for (auto &c: temp) {
            if (c == (Byte)altChars[0]) {
                c = '+';
            } else if (c == (Byte)altChars[1]) {
                c = '/';
            }
        }
    }
    std::copy(Base64DecodeIterator(temp.begin()), Base64DecodeIterator(temp.end()), std::back_inserter(result));
    return result;
}