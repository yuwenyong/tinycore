//
// Created by yuwenyong on 17-5-10.
//

#include "tinycore/compress/zlib.h"


void Zlib::handleError(z_stream zst, int err, const char *msg) {
    const char *zmsg = Z_NULL;
    if (err == Z_VERSION_ERROR) {
        zmsg = "library version mismatch";
    }
    if (zmsg == Z_NULL) {
        zmsg = zst.msg;
    }
    if (zmsg == Z_NULL) {
        switch (err) {
            case Z_BUF_ERROR:
                zmsg = "incomplete or truncated stream";
                break;
            case Z_STREAM_ERROR:
                zmsg = "inconsistent stream state";
                break;
            case Z_DATA_ERROR:
                zmsg = "invalid input data";
                break;
            default:
                break;
        }
    }
    if (zmsg == Z_NULL) {
        ThrowException(ZlibError, String::format("Error %d %s", err, msg));
    } else {
        ThrowException(ZlibError, String::format("Error %d %s: %.200s", err, msg, zmsg));
    }
}

CompressObj::CompressObj(int level, int method, int wbits, int memLevel, int strategy)
        : _inited(false)
        , _level(level)
        , _method(method)
        , _wbits(wbits)
        , _memLevel(memLevel)
        , _strategy(strategy) {
    _zst.zalloc = (alloc_func) NULL;
    _zst.zfree = (free_func) Z_NULL;
    _zst.next_in = NULL;
    _zst.avail_in = 0;
}

CompressObj::CompressObj(const CompressObj &rhs)
        : _inited(rhs._inited)
        , _level(rhs._level)
        , _method(rhs._method)
        , _wbits(rhs._wbits)
        , _memLevel(rhs._memLevel)
        , _strategy(rhs._strategy) {
    if (_inited) {
        int err = deflateCopy(&_zst, const_cast<z_stream *>(&rhs._zst));

        if (err != Z_OK) {
            _inited = false;
#ifndef NDEBUG
            if (err == Z_STREAM_ERROR) {
                ThrowException(ValueError, "Inconsistent stream state");
            } else if (err == Z_MEM_ERROR) {
                ThrowException(MemoryError, "Can't allocate memory for compression object");
            } else {
                Zlib::handleError(rhs._zst, err, "while copying compression object");
            }
#endif
        }
    } else {
        _zst.zalloc = (alloc_func) NULL;
        _zst.zfree = (free_func) Z_NULL;
        _zst.next_in = NULL;
        _zst.avail_in = 0;
    }
}

CompressObj::CompressObj(CompressObj &&rhs)
        : _inited(rhs._inited)
        , _level(rhs._level)
        , _method(rhs._method)
        , _wbits(rhs._wbits)
        , _memLevel(rhs._memLevel)
        , _strategy(rhs._strategy) {
    _zst = rhs._zst;
    if (rhs._inited) {
        rhs._inited = false;
        rhs._zst.zalloc = (alloc_func) NULL;
        rhs._zst.zfree = (free_func) Z_NULL;
        rhs._zst.next_in = NULL;
        rhs._zst.avail_in = 0;
    }
}

CompressObj& CompressObj::operator=(const CompressObj &rhs) {
    clear();
    _inited = rhs._inited;
    _level = rhs._level;
    _method = rhs._method;
    _wbits = rhs._wbits;
    _memLevel = rhs._memLevel;
    _strategy = rhs._strategy;
    if (_inited) {
        int err = deflateCopy(&_zst, const_cast<z_stream *>(&rhs._zst));
        if (err != Z_OK) {
            _inited = false;
            if (err == Z_STREAM_ERROR) {
                ThrowException(ValueError, "Inconsistent stream state");
            } else if (err == Z_MEM_ERROR) {
                ThrowException(MemoryError, "Can't allocate memory for compression object");
            } else {
                Zlib::handleError(rhs._zst, err, "while copying compression object");
            }
        }
    }
    return *this;
}

CompressObj& CompressObj::operator=(CompressObj &&rhs) {
    clear();
    _inited = rhs._inited;
    _level = rhs._level;
    _method = rhs._method;
    _wbits = rhs._wbits;
    _memLevel = rhs._memLevel;
    _strategy = rhs._strategy;
    _zst = rhs._zst;
    if (rhs._inited) {
        rhs._inited = false;
        rhs._zst.zalloc = (alloc_func) NULL;
        rhs._zst.zfree = (free_func) Z_NULL;
        rhs._zst.next_in = NULL;
        rhs._zst.avail_in = 0;
    }
    return *this;
}

void CompressObj::init() {
    if (!_inited) {
        int err = deflateInit2(&_zst, _level, _method, _wbits, _memLevel, _strategy);
        if (err == Z_OK) {
            _inited = true;
        } else if (err == Z_MEM_ERROR) {
            ThrowException(MemoryError, "Can't allocate memory for compression object");
        } else if (err == Z_STREAM_ERROR) {
            ThrowException(ValueError, "Invalid initialization option");
        } else {
            Zlib::handleError(_zst, err, "while creating compression object");
        }
    }
}

void CompressObj::clear() {
    if (_inited) {
        deflateEnd(&_zst);
        _zst.zalloc = (alloc_func) NULL;
        _zst.zfree = (free_func) Z_NULL;
        _zst.next_in = NULL;
        _zst.avail_in = 0;
        _inited = false;
    }
}