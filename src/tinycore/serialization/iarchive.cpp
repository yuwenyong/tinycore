//
// Created by yuwenyong on 17-9-7.
//

#include "tinycore/serialization/iarchive.h"
#include "tinycore/utilities/strutil.h"


IArchive& IArchive::operator>>(float &value) {
    value = read<float>();
    if (!std::isfinite(value)) {
        ThrowException(ArchiveError, "Infinite float value");
    }
    return *this;
}

IArchive& IArchive::operator>>(double &value) {
    value = read<double>();
    if (!std::isfinite(value)) {
        ThrowException(ArchiveError, "Infinite double value");
    }
    return *this;
}

IArchive& IArchive::operator>>(std::string &value) {
    size_t len = read<uint8>();
    if (len == 0xFF) {
        len = read<uint32>();
    }
    std::string result;
    if (len > 0) {
        result.resize(len);
        read((Byte *)result.data(), len);
    }
    value.swap(result);
    return *this;
}

IArchive& IArchive::operator>>(ByteArray &value) {
    size_t len = read<uint8>();
    if (len == 0xFF) {
        len = read<uint32>();
    }
    ByteArray result;
    if (len > 0) {
        result.resize(len);
        read(result.data(), len);
    }
    value.swap(result);
    return *this;
}

void IArchive::checkSkipOverflow(size_t len) const {
    if (_pos + len > _size) {
        std::string error = StrUtil::format("Attempted to skip value with size: %lu in IArchive (pos: %lu size: %lu)",
                                            len, _pos, _size);
        ThrowException(ArchivePositionError, error);
    }
}

void IArchive::checkReadOverflow(size_t len) const {
    if (_pos + len > _size) {
        std::string error = StrUtil::format("Attempted to skip value with size: %lu in IArchive (pos: %lu size: %lu)",
                                            len, _pos, _size);
        ThrowException(ArchivePositionError, error);
    }
}