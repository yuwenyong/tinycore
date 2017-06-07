//
// Created by yuwenyong on 17-4-18.
//

#include "tinycore/compress/gzip.h"
#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/filesystem.hpp>


GzipFile::~GzipFile() {
    close();
}

void GzipFile::initWithInputFile(const std::string &fileName) {
    checkInited();
    auto fileObj = std::make_shared<std::fstream>();
    fileObj->open(fileName.c_str(), std::fstream::in | std::fstream::binary);
    _mode = GzipFileMode::READ;
    _newMember = true;
    _extraSize = 0;
    _extraStart = 0;
    _name = fileName;
    _minReadSize = 100;
    _fileObj = std::move(fileObj);
    _offset = 0;
}

void GzipFile::initWithInputStream(std::shared_ptr<std::iostream> fileObj) {
    checkInited();
    _mode = GzipFileMode::READ;
    _newMember = true;
    _extraSize = 0;
    _extraStart = 0;
    _minReadSize = 100;
    _fileObj = std::move(fileObj);
    _offset = 0;
}

void GzipFile::initWithOutputFile(std::string fileName, std::ios_base::openmode mode, int compresslevel,
                                  time_t *mtime) {
    checkInited();
    if (!(mode & std::fstream::out)) {
        ThrowException(IOError, "Mode not supported");
    }
    auto fileObj = std::make_shared<std::fstream>();
    fileObj->open(fileName.c_str(), mode | std::fstream::binary);
    _mode = GzipFileMode::WRITE;
    initWrite(fileName);
    _compress = make_unique<CompressObj>(compresslevel, Zlib::deflated, -Zlib::maxWBits, Zlib::defMemLevel, 0);
    _fileObj = std::move(fileObj);
    _offset = 0;
    if (mtime) {
        _mtime = *mtime;
    }
    writeGzipHeader();
}

void GzipFile::initWithOutputStream(std::shared_ptr<std::iostream> fileObj, int compresslevel, time_t *mtime) {
    checkInited();
    _mode = GzipFileMode::WRITE;
    initWrite("");
    _compress = make_unique<CompressObj>(compresslevel, Zlib::deflated, -Zlib::maxWBits, Zlib::defMemLevel, 0);
    _fileObj = std::move(fileObj);
    _offset = 0;
    if (mtime) {
        _mtime = *mtime;
    }
    writeGzipHeader();
}

size_t GzipFile::write(const Byte *data, size_t len) {
    checkClosed();
    if (_mode != GzipFileMode::WRITE) {
        ThrowException(IOError, "write() on read-only GzipFile object");
    }
    if (!_fileObj) {
        ThrowException(ValueError, "write() on closed GzipFile object");
    }
    if (len > 0) {
        _size += len;
        _crc = Zlib::crc32(data, len, _crc) & 0xffffffff;
        ByteArray compressData = _compress->compress(data, len);
        std::cout << String::toHexStr(compressData) << std::endl;
        _fileObj->write((char *)compressData.data(), compressData.size());
        _offset += len;
    }
    return len;
}

ByteArray GzipFile::read(ssize_t size) {
    checkClosed();
    if (_mode != GzipFileMode::READ) {
        ThrowException(IOError, "read() on write-only GzipFile object");
    }
    ByteArray chunk;
    if (_extraSize <= 0 && !_fileObj) {
        return chunk;
    }
    size_t readSize = 1024;
    if (size < 0) {
        try {
            while (true) {
                internalRead(readSize);
                readSize = std::min(maxReadChunk, readSize << 1);
            }
        } catch (EOFError &e) {
            size = _extraSize;
        }
    } else {
        try {
            while (size > _extraSize) {
                internalRead(readSize);
                readSize = std::min(maxReadChunk, readSize << 1);
            }
        } catch (EOFError &e) {
            if (size > _extraSize) {
                size = _extraSize;
            }
        }
    }
    ssize_t offset = _offset - _extraStart;
    chunk.assign(std::next(_extraBuf.begin(), offset), std::next(_extraBuf.begin(), offset + size));
    _extraSize -= size;
    _offset += size;
    return chunk;
}

std::string GzipFile::readToString(ssize_t size) {
    checkClosed();
    if (_mode != GzipFileMode::READ) {
        ThrowException(IOError, "read() on write-only GzipFile object");
    }
    std::string chunk;
    if (_extraSize <= 0 && !_fileObj) {
        return chunk;
    }
    size_t readSize = 1024;
    if (size < 0) {
        try {
            while (true) {
                internalRead(readSize);
                readSize = std::min(maxReadChunk, readSize << 1);
            }
        } catch (EOFError &e) {
            size = _extraSize;
        }
    } else {
        try {
            while (size > _extraSize) {
                internalRead(readSize);
                readSize = std::min(maxReadChunk, readSize << 1);
            }
        } catch (EOFError &e) {
            if (size > _extraSize) {
                size = _extraSize;
            }
        }
    }
    ssize_t offset = _offset - _extraStart;
    chunk.assign((const char*)_extraBuf.data() + offset, size);
    _extraSize -= size;
    _offset += size;
    return chunk;
}

void GzipFile::close() {
    if (!_fileObj) {
        return;
    }
    if (_mode == GzipFileMode::WRITE) {
        std::string tail = _compress->flushToString();
        _fileObj->write(tail.data(), tail.size());
        uint32_t crc = (uint32_t)_crc;
        boost::endian::native_to_little_inplace(crc);
        _fileObj->write((const char *)&crc, 4);
        uint32_t size = (uint32_t)(_size & 0xffffffff);
        boost::endian::native_to_little(size);
        _fileObj->write((const char *)&size, 4);
        _fileObj->flush();
        if (!_name.empty()) {
            auto fileObj = std::dynamic_pointer_cast<std::fstream>(_fileObj);
            if (fileObj) {
                fileObj->close();
            }
        }
        _fileObj.reset();
    } else if (_mode == GzipFileMode::READ) {
        if (!_name.empty()) {
            auto fileObj = std::dynamic_pointer_cast<std::fstream>(_fileObj);
            if (fileObj) {
                fileObj->close();
            }
        }
        _fileObj.reset();
    }
    if (_fileObj) {
        if (!_name.empty()) {
            auto fileObj = std::dynamic_pointer_cast<std::fstream>(_fileObj);
            if (fileObj) {
                fileObj->close();
            }
        }
        _fileObj.reset();
    }
}

void GzipFile::flush(int mode) {
    checkClosed();
    if (_mode == GzipFileMode::WRITE) {
        std::string tail = _compress->flushToString(mode);
        _fileObj->write(tail.data(), tail.size());
        _fileObj->flush();
    }
}

void GzipFile::rewind() {
    if (_mode != GzipFileMode::READ) {
        ThrowException(IOError, "Can't rewind in write mode");
    }
    _fileObj->seekg(0, std::ios_base::beg);
    _newMember = true;
    _extraBuf.clear();
    _extraSize = 0;
    _extraStart = 0;
    _offset = 0;
}

ssize_t GzipFile::seek(ssize_t offset, std::ios_base::seekdir whence) {
    if (whence == std::ios_base::cur) {
        offset = _offset + offset;
    } else if (whence == std::ios_base::end) {
        ThrowException(ValueError, "Seek from end not supported");
    }
    if (_mode == GzipFileMode::WRITE) {
        if (offset < _offset) {
            ThrowException(IOError, "Negative seek in write mode");
        }
        ssize_t count = offset - _offset;
        ByteArray buf((size_t)count, '\0');
        write(buf);
    } else if (_mode == GzipFileMode::READ) {
        if (offset < _offset) {
            rewind();
        }
        ssize_t count = offset - _offset;
        read(count);
    }
    return _offset;
}

std::string GzipFile::readLine(ssize_t size) {
    ssize_t readSize;
    if (size < 0) {
        ssize_t offset = _offset - _extraStart;
        if (offset < (ssize_t)_extraBuf.size()) {
            auto beg = std::next(_extraBuf.begin(), offset);
            auto iter = std::find(beg, _extraBuf.end(), '\n');
            if (iter != _extraBuf.end()) {
                std::advance(iter, 1);
                ssize_t count = std::distance(beg, iter);
                _extraSize -= count;
                _offset += count;
                return std::string((const char *)_extraBuf.data() + offset, (size_t)count);
            }
        }
        size = INT32_MAX;
        readSize = _minReadSize;
    } else {
        readSize = size;
    }
    StringVector bufs;
    std::string c;
    size_t i;
    while (size != 0) {
        c = readToString(readSize);
        if (c.empty()) {
            break;
        }
        i = c.find('\n');
        if ((i != std::string::npos && size < (ssize_t)i) || (i == std::string::npos && (ssize_t)c.size() > size)) {
            bufs.emplace_back(c.data(), (size_t)size);
            unread(c.size() - size);
            break;
        }
        if (i != std::string::npos) {
            bufs.emplace_back(c.data(), i + 1);
            unread(c.size() - i - 1);
            break;
        }
        bufs.emplace_back(std::move(c));
        size -= c.size();
        readSize = std::min(size, readSize << 1);
    }
    if (readSize > _minReadSize) {
        _minReadSize = std::min(readSize, _minReadSize << 1);
        _minReadSize = std::min(_minReadSize, (ssize_t)512);
    }
    return boost::join(bufs, "");
}

const size_t GzipFile::maxReadChunk;

void GzipFile::initWrite(const std::string &fileName) {
    _name = fileName;
    _crc = Zlib::crc32("") & 0xffffffff;
    _size = 0;
//    _writeBuf.clear();
//    _bufSize = 0;
}

void GzipFile::writeGzipHeader() {
    const char magicHeader[] = {'\037', '\213'};
    _fileObj->write(magicHeader, sizeof(magicHeader));
    _fileObj->put('\010');
    std::string fname;
    if (!_name.empty()) {
        boost::filesystem::path file_name(_name);
        if (file_name.extension().string() == ".gz") {
            fname = file_name.stem().string();
        } else {
            fname = file_name.filename().string();
        }
    }
    char flags = (char)(fname.empty() ? 0 : FNAME);
    _fileObj->put(flags);
    uint32_t mtime;
    if (_mtime) {
        mtime = (uint32_t)_mtime.get();
    } else {
        mtime = (uint32_t)::time(nullptr);
    }
    boost::endian::native_to_little_inplace(mtime);
    _fileObj->write((const char *)&mtime, 4);
    _fileObj->put('\002');
    _fileObj->put('\377');
    if (!fname.empty()) {
        _fileObj->write(fname.c_str(), fname.size());
        _fileObj->put('\000');
    }
}

void GzipFile::readGzipHeader() {
    char magicHeader[2];
    _fileObj->read(magicHeader, sizeof(magicHeader));
    if (!_fileObj->good() || magicHeader[0] != '\037' || magicHeader[1] != '\213') {
        ThrowException(IOError, "Not a gzipped file");
    }
    char method;
    _fileObj->get(method);
    if (!_fileObj->good() || method != '\010') {
        ThrowException(IOError, "Unknown compression method");
    }
    char flag;
    _fileObj->get(flag);
    if (!_fileObj->good()) {
        ThrowException(IOError, "Not a gzipped file");
    }
    uint32_t mtime = 0;
    _fileObj->read((char *)&mtime, 4);
    if (!_fileObj->good()) {
        ThrowException(IOError, "Not a gzipped file");
    }
    boost::endian::little_to_native_inplace(mtime);
    _mtime = mtime;
    _fileObj->ignore(2);
    if (_fileObj->fail()) {
        ThrowException(IOError, "Not a gzipped file");
    }
    if (flag & FEXTRA) {
        int xlen = _fileObj->get();
        xlen += 256 * _fileObj->get();
        _fileObj->ignore(xlen);
        if (_fileObj->fail()) {
            ThrowException(IOError, "Not a gzipped file");
        }
    }
    if (flag & FNAME) {
        while (true) {
            if (_fileObj->get() == '\0' || !_fileObj->good()) {
                break;
            }

        }
        if (_fileObj->fail()) {
            ThrowException(IOError, "Not a gzipped file");
        }
    }
    if (flag & FCOMMENT) {
        while (true) {
            if (_fileObj->get() == '\0' || !_fileObj->good()) {
                break;
            }
        }
        if (_fileObj->fail()) {
            ThrowException(IOError, "Not a gzipped file");
        }
    }
    if (flag & FHCRC) {
        _fileObj->ignore(2);
        if (_fileObj->fail()) {
            ThrowException(IOError, "Not a gzipped file");
        }
    }
}

void GzipFile::internalRead(size_t len) {
    if (!_fileObj || _fileObj->eof()) {
        ThrowException(EOFError, "Reached EOF");
    }
    if (_newMember) {
        auto pos = _fileObj->tellg();
        _fileObj->seekg(0, std::ios_base::end);
        if (pos == _fileObj->tellg()) {
            ThrowException(EOFError, "Reached EOF");
        } else {
            _fileObj->seekg(pos, std::ios_base::beg);
        }
        initRead();
        readGzipHeader();
        _decompress = make_unique<DecompressObj>(-Zlib::maxWBits);
        _newMember = false;
    }
    ByteArray buf;
    buf.resize(len);
    _fileObj->read((char *)buf.data(), len);
    buf.resize(_fileObj->gcount());
    ByteArray uncompress;
    if (buf.empty()) {
        uncompress = _decompress->flush();
        readEOF();
        addReadData(uncompress);
        ThrowException(EOFError, "Reached EOF");
    }
    uncompress = _decompress->decompress(buf);
    addReadData(uncompress);
    const ByteArray &unusedData = _decompress->getUnusedData();
    if (!unusedData.empty()) {
        _fileObj->seekg(8 - (ssize_t)unusedData.size(), std::ios_base::cur);
        readEOF();
        _newMember = true;
    }
}

void GzipFile::addReadData(const ByteArray &data) {
    _crc = Zlib::crc32(data, _crc) & 0xffffffff;
    ssize_t offset = _offset - _extraStart;
    if (offset > 0) {
        _extraBuf.erase(_extraBuf.begin(), std::next(_extraBuf.begin(), offset));
    }
    _extraBuf.insert(_extraBuf.end(), data.begin(), data.end());
    _extraSize += data.size();
    _extraStart = _offset;
    _size += data.size();
}

void GzipFile::readEOF() {
    _fileObj->clear();
    _fileObj->seekg(-8, std::ios_base::cur);
    uint32_t crc = 0;
    _fileObj->read((char *)&crc, 4);
    boost::endian::little_to_native_inplace(crc);
    uint32 isize = 0;
    _fileObj->read((char *)&isize, 4);
    boost::endian::little_to_native_inplace(isize);
    if (crc != _crc) {
        ThrowException(IOError, String::format("CRC check failed %x != %x", crc, _crc));
    } else if (isize != (_size & 0xffffffff)) {
        ThrowException(IOError, "Incorrect length of data produced");
    }
    char c = '\x00';
    while (_fileObj->good() && c == '\x00') {
        c = (char)_fileObj->get();
    }
    if (_fileObj->good()) {
        _fileObj->seekg(-1, std::ios_base::cur);
    }
}