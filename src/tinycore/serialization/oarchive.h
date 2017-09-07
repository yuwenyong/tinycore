//
// Created by yuwenyong on 17-9-7.
//

#ifndef TINYCORE_OARCHIVE_H
#define TINYCORE_OARCHIVE_H

#include "tinycore/common/common.h"
#include "tinycore/serialization/iarchive.h"


class OArchive {
public:
    OArchive() = default;

    explicit OArchive(size_t reserve) {
        _storage.reserve(reserve);
    }

    ByteArray&& move() noexcept {
        return std::move(_storage);
    }

    bool empty() const {
        return _storage.empty();
    }

    size_t size() const {
        return _storage.size();
    }

    Byte* contents() {
        if (_storage.empty()) {
            ThrowException(ArchiveError, "Empty buffer");
        }
        return _storage.data();
    }

    const Byte* contents() const {
        if (_storage.empty()) {
            ThrowException(ArchiveError, "Empty buffer");
        }
        return _storage.data();
    }

    OArchive& operator<<(uint8 value) {
        append<uint8>(value);
        return *this;
    }

    OArchive& operator<<(uint16 value) {
        append<uint16>(value);
        return *this;
    }

    OArchive& operator<<(uint32 value) {
        append<uint32>(value);
        return *this;
    }

    OArchive& operator<<(uint64 value) {
        append<uint64>(value);
        return *this;
    }

    OArchive& operator<<(int8 value) {
        append<int8>(value);
        return *this;
    }

    OArchive& operator<<(int16 value) {
        append<int16>(value);
        return *this;
    }

    OArchive& operator<<(int32 value) {
        append<int32>(value);
        return *this;
    }

    OArchive& operator<<(int64 value) {
        append<int64>(value);
        return *this;
    }

    OArchive& operator<<(float value) {
        append<float>(value);
        return *this;
    }

    OArchive& operator<<(double value) {
        append<double>(value);
        return *this;
    }

    OArchive& operator<<(const char *value);

    OArchive& operator<<(const std::string &value);

    OArchive& operator<<(const ByteArray &value);

    template <size_t LEN>
    OArchive& operator<<(const std::array<char, LEN> &value) {
        append((const Byte *)value.data(), value.size());
        return *this;
    }

    template <size_t LEN>
    OArchive& operator<<(const std::array<Byte, LEN> &value) {
        append(value.data(), value.size());
        return *this;
    }

    template <typename ElemT, size_t LEN>
    OArchive& operator<<(std::array<ElemT, LEN> &value) {
        for (auto &elem: value) {
            *this << elem;
        }
        return *this;
    }

    template <typename ElemT>
    OArchive& operator<<(std::vector<ElemT> &value) {
        appendSequence(value);
        return *this;
    }

    template <typename ElemT>
    OArchive& operator<<(std::list<ElemT> &value) {
        appendSequence(value);
        return *this;
    }

    template <typename ElemT>
    OArchive& operator<<(std::deque<ElemT> &value) {
        appendSequence(value);
        return *this;
    }

    template <typename KeyT, typename ValueT>
    OArchive& operator<<(std::map<KeyT, ValueT> &value) {
        appendMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    OArchive& operator<<(std::multimap<KeyT, ValueT> &value) {
        appendMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    OArchive& operator<<(std::unordered_map<KeyT, ValueT> &value) {
        appendMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    OArchive& operator<<(std::unordered_multimap<KeyT, ValueT> &value) {
        appendMapping(value);
        return *this;
    };

    template <typename ValueT>
    OArchive& operator<<(ValueT &value) {
        value.serialize(*this);
        return *this;
    }

    template <typename ValueT>
    OArchive& operator&(ValueT &value) {
        return *this << value;
    }
protected:
    template <typename ElemT, template <typename> class ContainerT>
    void appendSequence(ContainerT<ElemT> &value) {
        size_t len = value.size();
        if (len >= 0xFF) {
            append<uint8>(0xff);
            append<uint32>(static_cast<uint32>(len));
        } else {
            append<uint8>(static_cast<uint8>(len));
        }
        for (ElemT &elem: value) {
            *this << elem;
        }
    }

    template <typename KeyT, typename ValueT, template <typename, typename> class ContainerT>
    void appendMapping(ContainerT<KeyT, ValueT> &value) {
        size_t len = value.size();
        if (len >= 0xFF) {
            append<uint8>(0xff);
            append<uint32>(static_cast<uint32>(len));
        } else {
            append<uint8>(static_cast<uint8>(len));
        }
        for (auto &kv: value) {
            *this << kv.first << kv.second;
        }
    };

    template <typename ValueT>
    void append(ValueT value) {
        static_assert(std::is_fundamental<ValueT>::value, "append(compound)");
        append((uint8 *)&value, sizeof(value));
    }

    void append(const Byte *src, size_t len) {
        _storage.insert(_storage.end(), src, src + len);
    }

    ByteArray _storage;
};

#endif //TINYCORE_OARCHIVE_H
