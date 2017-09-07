//
// Created by yuwenyong on 17-9-7.
//

#include "tinycore/serialization/oarchive.h"


OArchive& OArchive::operator<<(const char *value) {
    size_t len = strlen(value);
    if (len >= 0xff) {
        append<uint8>(0xff);
        append<uint32>(static_cast<uint32>(len));
    } else {
        append<uint8>(static_cast<uint8>(len));
    }
    append((const Byte *)value, len);
    return *this;
}

OArchive& OArchive::operator<<(const std::string &value) {
    size_t len = value.size();
    if (len >= 0xff) {
        append<uint8>(0xff);
        append<uint32>(static_cast<uint32>(len));
    } else {
        append<uint8>(static_cast<uint8>(len));
    }
    append((const Byte *)value.data(), len);
    return *this;
}

OArchive& OArchive::operator<<(const ByteArray &value) {
    size_t len = value.size();
    if (len >= 0xff) {
        append<uint8>(0xff);
        append<uint32>(static_cast<uint32>(len));
    } else {
        append<uint8>(static_cast<uint8>(len));
    }
    append(value.data(), len);
    return *this;
}