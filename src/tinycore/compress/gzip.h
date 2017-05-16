//
// Created by yuwenyong on 17-4-18.
//

#ifndef TINYCORE_GZIP_H
#define TINYCORE_GZIP_H

#include "tinycore/common/common.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/optional.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/compress/zlib.h"

namespace io = boost::iostreams;


class GZipCompressor {
public:
    GZipCompressor(const std::string &fileName, int compressLevel=9);
    GZipCompressor(std::vector<char> &buffer, int compressLevel=9);

    void write(const std::vector<char> &data) {
        checkClosed();
        io::write(_stream, data.data(), data.size());
    }

    void write(const std::string &data) {
        checkClosed();
        io::write(_stream, data.c_str(), data.length());
    }

    void flush() {
        checkClosed();
        io::flush(_stream);
    }

    void close() {
        if (!_closed) {
            io::close(_stream);
            _closed = true;
        }
    }
protected:
    void checkClosed() {
        if (_closed) {
            ThrowException(ValueError, "I/O operation on closed file");
        }
    }

    io::gzip_params _params;
    io::gzip_compressor _compressor;
    io::filtering_ostream _stream;
    bool _closed{false};
};


class GZipDecompressor {
public:
    GZipDecompressor(const std::string &fileName);
    GZipDecompressor(const std::vector<char> &data);

    ssize_t read(std::vector<char> &buffer);
    ssize_t read(std::string &buffer);
    ssize_t read(char *buffer, ssize_t size);

    void close() {
        if (!_closed) {
            io::close(_stream);
            _closed = true;
        }
    }
protected:
    void checkClosed() {
        if (_closed) {
            ThrowException(ValueError, "I/O operation on closed file");
        }
    }

    io::gzip_decompressor _decompressor;
    io::filtering_istream _stream;
    bool _closed{false};
};


enum class GzipFileMode {
    NONE,
    READ,
    WRITE
};

typedef std::shared_ptr<std::iostream> BasicStreamPtr;
typedef std::shared_ptr<std::stringstream> StringStreamPtr;
typedef std::shared_ptr<std::fstream> FileStreamPtr;

#define FTEXT 1
#define FHCRC 2
#define FEXTRA 4
#define FNAME 8
#define FCOMMENT 16

class GzipFile {
public:
    GzipFile() {}
    ~GzipFile();

    void initWithInputFile(const std::string &fileName);
    void initWithInputStream(std::shared_ptr<std::iostream> fileObj);
    void initWithOutputFile(std::string fileName, std::ios_base::openmode mode=std::fstream::out|std::fstream::trunc,
                            int compresslevel=9, time_t *mtime= nullptr);
    void initWithOutputStream(std::shared_ptr<std::iostream> fileObj, int compresslevel=9, time_t *mtime= nullptr);
    size_t write(const Byte *data, size_t len);

    size_t write(const ByteArray &data) {
        return write(data.data(), data.size());
    }

    size_t write(const char * data) {
        return write((const Byte *)data, strlen(data));
    }

    size_t write(const std::string &data) {
        return write((const Byte *)data.data(), data.size());
    }

    ByteArray read(ssize_t size=-1);
    std::string readToString(ssize_t size=-1);

    bool closed() const {
        return !_fileObj;
    }

    void close();
    void flush(int mode=Zlib::zSyncFlush);
    void rewind();

    bool readable() const {
        return _mode == GzipFileMode::READ;
    }

    bool writeable() const {
        return _mode == GzipFileMode::WRITE;
    }

    bool seekable() const {
        return true;
    }

    ssize_t seek(ssize_t offset, std::ios_base::seekdir whence=std::ios_base::beg);
    std::string readLine(ssize_t size=-1);

    static const size_t maxReadChunk = 10 * 1024 * 1024;
protected:
    void checkInited() const {
        if (_mode == GzipFileMode::READ) {
            ThrowException(IOError, "ReOpen on read-only GzipFile object");
        } else if (_mode == GzipFileMode::WRITE) {
            ThrowException(IOError, "ReOpen on write-only GzipFile object");
        }
    }

    void checkClosed() const {
        if (!_fileObj) {
            ThrowException(ValueError, "I/O operation on closed file");
        }
    }

    void initWrite(const std::string &fileName);
    void writeGzipHeader();

    void initRead() {
        _crc = Zlib::crc32("") & 0xffffffff;
        _size = 0;
    }

    void readGzipHeader();

    void unread(size_t len) {
        _extraSize += len;
        _offset -= len;
    }

    void internalRead(size_t len=1024);
    void addReadData(const ByteArray &data);
    void readEOF();

    GzipFileMode _mode{GzipFileMode::NONE};
    bool _newMember{false};
    ByteArray _extraBuf;
    ssize_t _extraSize{0};
    ssize_t _extraStart{0};
    std::string _name;
    ssize_t _minReadSize{0};
    std::shared_ptr<std::iostream> _fileObj;
    ssize_t _offset{0};
    boost::optional<time_t> _mtime;
    std::unique_ptr<CompressObj> _compress;
    std::unique_ptr<DecompressObj> _decompress;
    unsigned long _crc;
    size_t _size{0};
//    ByteArray _writeBuf;
//    size_t _bufSize{0};
};


#endif //TINYCORE_GZIP_H
