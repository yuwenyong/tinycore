//
// Created by yuwenyong on 17-4-18.
//

#include "tinycore/compress/gzip.h"
#include <boost/iostreams/device/file.hpp>
#include <boost/range/iterator_range.hpp>


GZipCompressor::GZipCompressor(const std::string &fileName, int compressLevel)
        : _params(compressLevel, io::zlib::deflated, io::zlib::default_window_bits, io::zlib::default_mem_level,
                  io::zlib::default_strategy, fileName)
        , _compressor(_params)
        , _stream(_compressor | io::file_sink(fileName)) {

}

GZipCompressor::GZipCompressor(std::vector<char> &buffer, int compressLevel)
        : _params(compressLevel)
        , _compressor(_params)
        , _stream(_compressor | io::back_inserter(buffer)) {

}


GZipDecompressor::GZipDecompressor(const std::string &fileName)
        : _decompressor()
        , _stream(_decompressor | io::file_source(fileName, std::ios_base::in | std::ios_base::binary)) {

}

GZipDecompressor::GZipDecompressor(const std::vector<char> &data)
        : _decompressor()
        , _stream(_decompressor | boost::make_iterator_range(data)) {

}

ssize_t GZipDecompressor::read(std::vector<char> &buffer) {
    checkClosed();
    ssize_t length = io::copy(_stream, io::back_inserter(buffer));
    _closed = true;
    return length;
}

ssize_t GZipDecompressor::read(std::string &buffer) {
    checkClosed();
    ssize_t count = io::copy(_stream, io::back_inserter(buffer));
    _closed = true;
    return count;
}

ssize_t GZipDecompressor::read(char *buffer, ssize_t size) {
    checkClosed();
    ssize_t length = io::read(_stream, buffer, size);
    if (length < 0) {
        _closed = true;
    }
    return length;
}
