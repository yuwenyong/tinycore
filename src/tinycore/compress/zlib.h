//
// Created by yuwenyong on 17-5-10.
//

#ifndef TINYCORE_ZLIB_H
#define TINYCORE_ZLIB_H

#include "tinycore/common/common.h"
#include <zlib.h>
#include "tinycore/common/errors.h"
#include "tinycore/debugging/trace.h"


#define DEFLATED   8
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
#define DEF_WBITS MAX_WBIT
#define DEFAULTALLOC (16*1024)


DECLARE_EXCEPTION(ZlibError, Exception);


class Zlib {
public:
    static int adler32(const char *data, size_t len, unsigned int value=1) {
        return ::adler32(value, (const Bytef *)data, len);
    }

    static int adler32(const std::string &data, unsigned int value=1) {
        return adler32(data.data(), data.size(), value);
    }

    static int adler32(const std::vector<char> &data, unsigned int value=1) {
        return adler32(data.data(), data.size(), value);
    }

    static int crc32(const char *data, size_t len, unsigned int value=0) {
        return ::crc32(value, (const Bytef *)data, len);
    }

    static int crc32(const std::string &data, unsigned int value=0) {
        return crc32(data.data(), data.size(), value);
    }

    static int crc32(const std::vector<char> &data, unsigned int value=0) {
        return crc32(data.data(), data.size(), value);
    }

    template <typename T>
    static void compress(const char *data, size_t len, T &outVal, int level=zDefaultCompression) {
        z_stream zst;
        zst.zalloc = (alloc_func)NULL;
        zst.zfree = (free_func)Z_NULL;
        zst.next_in = (Bytef *)data;
        size_t oBufLen = DEFAULTALLOC, occupied = 0;
        T retVal;
        int err = deflateInit(&zst, level);
        if (err != Z_OK) {
            if (err == Z_MEM_ERROR) {
                ThrowException(MemoryError, "Out of memory while compressing data");
            } else if (err == Z_STREAM_ERROR) {
                ThrowException(ZlibError, "Bad compression level");
            } else {
                deflateEnd(&zst);
                handleError(zst, err, "while compressing data");
            }
        }
        zst.avail_in = len;
        do {
            if (retVal.empty()) {
                occupied = 0;
            } else {
                occupied = zst.next_out - (Bytef *)retVal.data();
                oBufLen <<= 1;
            }
            retVal.resize(oBufLen);
            zst.avail_out = oBufLen - occupied;
            zst.next_out = (Bytef *)retVal.data() + occupied;
            err = deflate(&zst, Z_FINISH);
            if (err == Z_STREAM_ERROR) {
                deflateEnd(&zst);
                ThrowException(ZlibError, "while compressing data");
            }
        } while (zst.avail_out == 0);
        ASSERT(zst.avail_in == 0);
        ASSERT(err == Z_STREAM_END);
        err = deflateEnd(&zst);
        if (err != Z_OK) {
            handleError(zst, err, "while finishing compression");
        }
        retVal.resize(zst.next_out - (Bytef *)retVal.data());
        retVal.swap(outVal);
    }

    template <typename T>
    static void compress(const std::string &data, T &outVal, int level=zDefaultCompression) {
        compress(data.data(), data.size(), outVal, level);
    }

    template <typename T>
    static void compress(const std::vector<char> &data, T &outVal, int level=zDefaultCompression) {
        compress(data.data(), data.size(), outVal, level);
    }

    static std::string compress(const char *data, size_t len, int level=zDefaultCompression) {
        std::string retVal;
        compress(data, len, retVal, level);
        return retVal;
    }

    static std::string compress(const std::string &data, int level=zDefaultCompression) {
        return compress(data.data(), data.size(), level);
    }

    static std::string compress(const std::vector<char> &data, int level=zDefaultCompression) {
        return compress(data.data(), data.size(), level);
    }

    template <typename T>
    static void decompress(const char *data, size_t len, T &outVal, int wbits=maxWBits) {
        z_stream zst;
        zst.zalloc = (alloc_func)NULL;
        zst.zfree = (free_func)Z_NULL;
        zst.avail_in = 0;
        zst.next_in = (Bytef *)data;
        size_t oBufLen = DEFAULTALLOC, occupied = 0;
        T retVal;
        int err = inflateInit2(&zst, wbits);
        if (err != Z_OK) {
            if (err == Z_MEM_ERROR) {
                ThrowException(MemoryError, "Out of memory while decompressing data");
            } else {
                inflateEnd(&zst);
                handleError(zst, err, "while preparing to decompress data");
            }
        }
        zst.avail_in = len;
        retVal.clear();
        do {
            if (retVal.empty()) {
                occupied = 0;
            } else {
                occupied = zst.next_out - (Bytef *)retVal.data();
                oBufLen <<= 1;
            }
            retVal.resize(oBufLen);
            zst.avail_out = oBufLen - occupied;
            zst.next_out = (Bytef *)retVal.data() + occupied;
            err = inflate(&zst, Z_FINISH);
            if (err != Z_OK && err != Z_BUF_ERROR && err != Z_STREAM_END) {
                inflateEnd(&zst);
                if (err == Z_MEM_ERROR) {
                    ThrowException(MemoryError, "Out of memory while decompressing data");
                } else {
                    handleError(zst, err, "while decompressing data");
                }
            }
        } while (zst.avail_out == 0);
        if (err != Z_STREAM_END) {
            inflateEnd(&zst);
            handleError(zst, err, "while decompressing data");
        }
        err = inflateEnd(&zst);
        if (err != Z_OK) {
            handleError(zst, err, "while finishing data decompression");
        }
        retVal.resize(zst.next_out - (Bytef *)retVal.data());
        retVal.swap(outVal);
    }

    template <typename T>
    static void decompress(const std::string &data, T &outVal, int wbits=maxWBits) {
        decompress(data.data(), data.size(), outVal, wbits);
    }

    template <typename T>
    static void decompress(const std::vector<char> &data, T &outVal, int wbits=maxWBits) {
        decompress(data.data(), data.size(), outVal, wbits);
    }

    static std::string decompress(const char *data, size_t len, int wbits=maxWBits) {
        std::string retVal;
        decompress(data, len, retVal, wbits);
        return retVal;
    }

    static std::string decompress(const std::string &data, int wbits=maxWBits) {
        return decompress(data.data(), data.size(), wbits);
    }

    static std::string decompress(const std::vector<char> &data, int wbits=maxWBits) {
        return decompress(data.data(), data.size(), wbits);
    }

    static void handleError(z_stream zst, int err, const char *msg);

    static const int maxWBits = MAX_WBITS;
    static const int deflated = DEFLATED;
    static const int defMemLevel = DEF_MEM_LEVEL;
    static const int zBestSpeed = Z_BEST_SPEED;
    static const int zBestCompression = Z_BEST_COMPRESSION;
    static const int zDefaultCompression = Z_DEFAULT_COMPRESSION;
    static const int zFiltered = Z_FILTERED;
    static const int zHuffmanOnly = Z_HUFFMAN_ONLY;
    static const int zDefaultStrategy = Z_DEFAULT_STRATEGY;

    static const int zFinish = Z_FINISH;
    static const int zNoFlush = Z_NO_FLUSH;
    static const int zSyncFlush = Z_SYNC_FLUSH;
    static const int zFullFlush = Z_FULL_FLUSH;
};


class CompressObj {
public:
    CompressObj(int level=Zlib::zDefaultCompression, int method=Zlib::deflated, int wbits=Zlib::maxWBits,
                int memLevel=Zlib::defMemLevel, int strategy=0);
    CompressObj(const CompressObj &rhs);
    CompressObj(CompressObj &&rhs);
    CompressObj& operator=(const CompressObj &rhs);
    CompressObj& operator=(CompressObj &&rhs);

    ~CompressObj() {
        clear();
    }

    template <typename T>
    void compress(const char *data, size_t len, T &outVal) {
        _zst.next_in = (Bytef *)data;
        size_t oBufLen = DEFAULTALLOC, occupied = 0;
        int err;
        T retVal;
        _zst.avail_in = len;
        do {
            if (retVal.empty()) {
                occupied = 0;
            } else {
                occupied = _zst.next_out - (Bytef *)retVal.data();
                oBufLen <<= 1;
            }
            retVal.resize(oBufLen);
            _zst.avail_out = oBufLen - occupied;
            _zst.next_out = (Bytef *)retVal.data() + occupied;
            err = deflate(&_zst, Z_NO_FLUSH);
            if (err == Z_STREAM_ERROR) {
                Zlib::handleError(_zst, err, "while compressing data");
            }
        } while (_zst.avail_out == 0);
        ASSERT(_zst.avail_in == 0);
        retVal.resize(_zst.next_out - (Bytef *)retVal.data());
        retVal.swap(outVal);
    }

    template <typename T>
    void compress(const std::string &data, T &outVal) {
        compress(data.data(), data.size(), outVal);
    }

    template <typename T>
    void compress(const std::vector<char> &data, T &outVal) {
        compress(data.data(), data.size(), outVal);
    }

    std::string compress(const char *data, size_t len) {
        std::string retVal;
        compress(data, len, retVal);
        return retVal;
    }

    std::string compress(const std::string &data) {
        return compress(data.data(), data.size());
    }

    std::string compress(const std::vector<char> &data) {
        return compress(data.data(), data.size());
    }

    template <typename T>
    void flush(T &outVal, int flushMode=Z_FINISH) {
        size_t oBufLen = DEFAULTALLOC, occupied = 0;
        int err;
        T retVal;
        if (flushMode == Z_NO_FLUSH) {
            retVal.swap(outVal);
            return;
        }
        _zst.avail_in = 0;
        do {
            if (retVal.empty()) {
                occupied = 0;
            } else {
                occupied = _zst.next_out - (Bytef *)retVal.data();
                oBufLen <<= 1;
            }
            retVal.resize(oBufLen);
            _zst.avail_out = oBufLen - occupied;
            _zst.next_out = (Bytef *)retVal.data() + occupied;
            err = deflate(&_zst, flushMode);
            if (err == Z_STREAM_ERROR) {
                Zlib::handleError(_zst, err, "while flushing");
            }
        } while (_zst.avail_out == 0);
        ASSERT(_zst.avail_in == 0);
        if (err == Z_STREAM_END && flushMode == Z_FINISH) {
            err = deflateEnd(&_zst);
            if (err != Z_OK) {
                Zlib::handleError(_zst, err, "from deflateEnd()");
            } else {
                _inited = false;
            }
        } else if (err != Z_OK && err != Z_BUF_ERROR) {
            Zlib::handleError(_zst, err, "while flushing");
        }
        retVal.resize(_zst.next_out - (Bytef *)retVal.data());
        retVal.swap(outVal);
    }

    std::string flush(int flushMode=Z_FINISH) {
        std::string retVal;
        flush(retVal, flushMode);
        return retVal;
    }
protected:
    void init();
    void clear();

    bool _inited{false};
    int _level;
    int _method;
    int _wbits;
    int _memLevel;
    int _strategy;
    z_stream _zst;
};


class DecompressObj {

};


#endif //TINYCORE_ZLIB_H
