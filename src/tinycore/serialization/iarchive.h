//
// Created by yuwenyong on 17-9-7.
//

#ifndef TINYCORE_IARCHIVE_H
#define TINYCORE_IARCHIVE_H

#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"


DECLARE_EXCEPTION(ArchiveError, Exception);
DECLARE_EXCEPTION(ArchivePositionError, ArchiveError);


class IArchive {
public:
    IArchive(const Byte *data, size_t size)
            : _storage(data)
            , _size(size) {

    }

    explicit IArchive(const ByteArray &data)
            : _storage(data.data())
            , _size(data.size()) {

    }

    bool empty() const {
        return _pos == _size;
    }

    size_t size() const {
        return _size;
    }

    size_t remainSize() const {
        return _size - _pos;
    }

    void ignore(size_t skip) {
        checkSkipOverflow(skip);
        _pos += skip;
    }

    IArchive& operator>>(bool &value) {
        value = read<char>() > 0;
        return *this;
    }

    IArchive& operator>>(uint8 &value) {
        value = read<uint8>();
        return *this;
    }

    IArchive& operator>>(uint16 &value) {
        value = read<uint16>();
        return *this;
    }

    IArchive& operator>>(uint32 &value) {
        value = read<uint32>();
        return *this;
    }

    IArchive& operator>>(uint64 &value) {
        value = read<uint64>();
        return *this;
    }

    IArchive& operator>>(int8 &value) {
        value = read<int8>();
        return *this;
    }

    IArchive& operator>>(int16 &value) {
        value = read<int16>();
        return *this;
    }

    IArchive& operator>>(int32 &value) {
        value = read<int32>();
        return *this;
    }

    IArchive& operator>>(int64 &value) {
        value = read<int64>();
        return *this;
    }

    IArchive& operator>>(float &value);

    IArchive& operator>>(double &value);

    IArchive& operator>>(std::string &value);

    IArchive& operator>>(ByteArray &value);

    template <size_t LEN>
    IArchive& operator>>(std::array<char, LEN> &value) {
        read((Byte *)value.data(), value.size());
        return *this;
    }

    template <size_t LEN>
    IArchive& operator>>(std::array<Byte, LEN> &value) {
        read(value.data(), value.size());
        return *this;
    }

    template <typename ElemT, size_t LEN>
    IArchive& operator>>(std::array<ElemT, LEN> &value) {
        for (auto &elem: value) {
            *this >> elem;
        }
        return *this;
    }

    template <typename ElemT>
    IArchive& operator>>(std::vector<ElemT> &value) {
        readSequence(value);
        return *this;
    }

    template <typename ElemT>
    IArchive& operator>>(std::list<ElemT> &value) {
        readSequence(value);
        return *this;
    }

    template <typename ElemT>
    IArchive& operator>>(std::deque<ElemT> &value) {
        readSequence(value);
        return *this;
    }

    template <typename KeyT, typename ValueT>
    IArchive& operator>>(std::map<KeyT, ValueT> &value) {
        readMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    IArchive& operator>>(std::multimap<KeyT, ValueT> &value) {
        readMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    IArchive& operator>>(std::unordered_map<KeyT, ValueT> &value) {
        readMapping(value);
        return *this;
    };

    template <typename KeyT, typename ValueT>
    IArchive& operator>>(std::unordered_multimap<KeyT, ValueT> &value) {
        readMapping(value);
        return *this;
    };

    template <typename ValueT>
    IArchive& operator>>(ValueT &value) {
        value.serialize(*this);
        return *this;
    }

    template <typename ValueT>
    IArchive& operator&(ValueT &value) {
        return *this >> value;
    }
protected:
    template <typename ElemT, template <typename> class ContainerT>
    void readSequence(ContainerT<ElemT> &value) {
        size_t len = read<uint8>();
        if (len == 0xFF) {
            len = read<uint32>();
        }
        ContainerT<ElemT> result;
        ElemT elem;
        for (size_t i = 0; i != len; ++i) {
            *this >> elem;
            result.emplace_back(std::move(elem));
        }
        result.swap(value);
    }

    template <typename KeyT, typename ValueT, template <typename, typename> class ContainerT>
    void readMapping(ContainerT<KeyT, ValueT> &value) {
        size_t len = read<uint8>();
        if (len == 0xFF) {
            len = read<uint32>();
        }
        ContainerT<KeyT, ValueT> result;
        KeyT key;
        ValueT val;
        for (size_t i = 0; i != len; ++i) {
            *this >> key >> val;
            result.emplace(std::move(key), std::move(val));
        }
        result.swap(value);
    };

    template <typename ValueT>
    ValueT read() {
        checkReadOverflow(sizeof(ValueT));
        ValueT val = *((ValueT const*)_storage + _pos);
        _pos += sizeof(ValueT);
        return val;
    }

    void read(Byte *dest, size_t len) {
        checkReadOverflow(len);
        std::memcpy(dest, _storage + _pos, len);
        _pos += len;
    }

    void checkReadOverflow(size_t len) const;

    void checkSkipOverflow(size_t len) const;

    const Byte *_storage;
    size_t _pos{0};
    size_t _size;
};


#endif //TINYCORE_IARCHIVE_H
