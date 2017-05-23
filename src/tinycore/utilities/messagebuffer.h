//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_MESSAGEBUFFER_H
#define TINYCORE_MESSAGEBUFFER_H

#include "tinycore/common/common.h"


class TC_COMMON_API MessageBuffer {
public:
    MessageBuffer()
            : _wpos(0),
              _rpos(0),
              _storage() {
        _storage.resize(4096);
    }

    explicit MessageBuffer(size_t initialSize)
            : _wpos(0),
              _rpos(0),
              _storage() {
        _storage.resize(initialSize);
    }

    MessageBuffer(MessageBuffer const& right)
            : _wpos(right._wpos),
              _rpos(right._rpos),
              _storage(right._storage) {
    }

    MessageBuffer(MessageBuffer&& right) noexcept
            : _wpos(right._wpos),
              _rpos(right._rpos),
              _storage(right.move()) {

    }

    void reset() {
        _wpos = 0;
        _rpos = 0;
    }

    void resize(size_t bytes) {
        _storage.resize(bytes);
    }

    Byte* getBasePointer() {
        return _storage.data();
    }

    Byte* getReadPointer() {
        return getBasePointer() + _rpos;
    }

    Byte* getWritePointer() {
        return getBasePointer() + _wpos;
    }

    void readCompleted(size_t bytes) {
        _rpos += bytes;
    }

    void writeCompleted(size_t bytes) {
        _wpos += bytes;
    }

    size_t getActiveSize() const {
        return _wpos - _rpos;
    }

    size_t getRemainingSpace() const {
        return _storage.size() - _wpos;
    }

    size_t getBufferSize() const {
        return _storage.size();
    }

    void normalize() {
        if (_rpos) {
            if (_rpos != _wpos) {
                memmove(getBasePointer(), getReadPointer(), getActiveSize());
            }
            _wpos -= _rpos;
            _rpos = 0;
        }
    }

    void ensureFreeSpace() {
        if (getRemainingSpace() == 0) {
            _storage.resize(_storage.size() * 3 / 2);
        }
    }

    void write(const Byte* data, size_t size) {
        if (size) {
            memcpy(getWritePointer(), data, size);
            writeCompleted(size);
        }
    }

    ByteArray&& move() {
        _wpos = 0;
        _rpos = 0;
        return std::move(_storage);
    }

    MessageBuffer& operator=(MessageBuffer const& right) {
        if (this != &right) {
            _wpos = right._wpos;
            _rpos = right._rpos;
            _storage = right._storage;
        }
        return *this;
    }

    MessageBuffer& operator=(MessageBuffer&& right) noexcept {
        if (this != &right) {
            _wpos = right._wpos;
            _rpos = right._rpos;
            _storage = right.move();
        }
        return *this;
    }

protected:
    size_t _wpos;
    size_t _rpos;
    ByteArray _storage;
};

#endif //TINYCORE_MESSAGEBUFFER_H
