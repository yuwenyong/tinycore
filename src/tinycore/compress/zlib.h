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
    static int adler32(const Byte *data, size_t len, unsigned int value=1) {
        return ::adler32(value, (Bytef *)data, len);
    }

    static int adler32(const ByteArray &data, unsigned int value=1) {
        return adler32(data.data(), data.size(), value);
    }

    static int adler32(const char *data, unsigned int value=1) {
        return adler32((const Byte *)data, strlen(data), value);
    }

    static int adler32(const std::string &data, unsigned int value=1) {
        return adler32((const Byte *)data.data(), data.size(), value);
    }

    static int crc32(const Byte *data, size_t len, unsigned int value=0) {
        return ::crc32(value, (const Bytef *)data, len);
    }

    static int crc32(const ByteArray &data, unsigned int value=0) {
        return crc32(data.data(), data.size(), value);
    }

    static int crc32(const char *data, unsigned int value=0) {
        return crc32((const Byte *)data, strlen(data), value);
    }

    static int crc32(const std::string &data, unsigned int value=0) {
        return crc32((const Byte *)data.data(), data.size(), value);
    }

    static ByteArray compress(const Byte *data, size_t len, int level=zDefaultCompression);

    static ByteArray compress(const ByteArray &data, int level=zDefaultCompression) {
        return compress(data.data(), data.size(), level);
    }

    static ByteArray compress(const char *data, int level=zDefaultCompression) {
        return compress((const Byte *)data, strlen(data), level);
    }

    static ByteArray compress(const std::string &data, int level=zDefaultCompression) {
        return compress((const Byte *)data.data(), data.size(), level);
    }

    static std::string compressToString(const Byte *data, size_t len, int level=zDefaultCompression);

    static std::string compressToString(const ByteArray &data, int level=zDefaultCompression) {
        return compressToString(data.data(), data.size(), level);
    }

    static std::string compressToString(const char* data, int level=zDefaultCompression) {
        return compressToString((const Byte *)data, strlen(data), level);
    }

    static std::string compressToString(const std::string &data, int level=zDefaultCompression) {
        return compressToString((const Byte *)data.data(), data.length(), level);
    }

    static ByteArray decompress(const Byte *data, size_t len, int wbits=maxWBits);

    static ByteArray decompress(const ByteArray &data, int wbits=maxWBits) {
        return decompress(data.data(), data.size(), wbits);
    }

    static ByteArray decompress(const std::string &data, int wbits=maxWBits) {
        return decompress((const Byte *)data.data(), data.size(), wbits);
    }

    static std::string decompressToString(const Byte *data, size_t len, int wbits=maxWBits);

    static std::string decompressToString(const ByteArray &data, int wbits=maxWBits) {
        return decompressToString(data.data(), data.size(), wbits);
    }

    static std::string decompressToString(const std::string &data, int wbits=maxWBits) {
        return decompressToString((const Byte *)data.data(), data.size(), wbits);
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
                int memLevel=Zlib::defMemLevel, int strategy=Zlib::zDefaultStrategy);
    CompressObj(const CompressObj &rhs);
    CompressObj(CompressObj &&rhs);
    CompressObj& operator=(const CompressObj &rhs);
    CompressObj& operator=(CompressObj &&rhs);

    ~CompressObj() {
        clear();
    }

    ByteArray compress(const Byte *data, size_t len);

    ByteArray compress(const ByteArray &data) {
        return compress(data.data(), data.size());
    }

    ByteArray compress(const char *data) {
        return compress((const Byte *)data, strlen(data));
    }

    ByteArray compress(const std::string &data) {
        return compress((const Byte *)data.data(), data.size());
    }

    std::string compressToString(const Byte *data, size_t len);

    std::string compressToString(const ByteArray &data) {
        return compressToString(data.data(), data.size());
    }

    std::string compressToString(const char *data) {
        return compressToString((const Byte *)data, strlen(data));
    }

    std::string compressToString(const std::string &data) {
        return compressToString((const Byte *)data.data(), data.size());
    }

    ByteArray flush(int flushMode=Zlib::zFinish);
    std::string flushToString(int flushMode=Zlib::zFinish);
protected:
    void initialize();
    void clear();

    bool _inited;
    int _level;
    int _method;
    int _wbits;
    int _memLevel;
    int _strategy;
    z_stream _zst;
};


class DecompressObj {
public:
    DecompressObj(int wbits=Zlib::maxWBits);
    DecompressObj(const DecompressObj &rhs);
    DecompressObj(DecompressObj &&rhs);
    DecompressObj& operator=(const DecompressObj &rhs);
    DecompressObj& operator=(DecompressObj &&rhs);

    ~DecompressObj() {
        clear();
    }

    ByteArray decompress(const Byte *data, size_t len, size_t maxLength=0);

    ByteArray decompress(const ByteArray &data, size_t maxLength=0) {
        return decompress(data.data(), data.size(), maxLength);
    }

    ByteArray decompress(const std::string &data, size_t maxLength=0) {
        return decompress((const Byte *)data.data(), data.size(), maxLength);
    }

    std::string decompressToString(const Byte *data, size_t len, size_t maxLength=0);

    std::string decompressToString(const ByteArray &data, size_t maxLength=0) {
        return decompressToString(data.data(), data.size(), maxLength);
    }

    std::string decompressToString(const std::string &data, size_t maxLength=0) {
        return decompressToString((const Byte *)data.data(), data.size(), maxLength);
    }

    ByteArray flush();
    std::string flushToString();

    const ByteArray& getUnusedData() const {
        return _unusedData;
    }

    const ByteArray& getUnconsumedTail() const {
        return _unconsumedTail;
    }
protected:
    void saveUnconsumedInput(const Byte *data, size_t len, int err);
    void initialize();
    void clear();

    bool _inited;
    int _wbits;
    z_stream _zst;
    ByteArray _unusedData;
    ByteArray _unconsumedTail;
};


#endif //TINYCORE_ZLIB_H
