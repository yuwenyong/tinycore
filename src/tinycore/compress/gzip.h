//
// Created by yuwenyong on 17-4-18.
//

#ifndef TINYCORE_GZIP_H
#define TINYCORE_GZIP_H

#include "tinycore/common/common.h"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include "tinycore/common/errors.h"

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


#endif //TINYCORE_GZIP_H
