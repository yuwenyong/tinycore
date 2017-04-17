//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/networking/iostream.h"
#include "tinycore/networking/httpserver.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"


BaseIOStream::BaseIOStream(SocketType &&socket, IOLoop *ioloop, size_t maxBufferSize, size_t readChunkSize)
        : _socket(std::move(socket))
        , _ioloop(ioloop)
        , _maxBufferSize(maxBufferSize)
        , _readBuffer(readChunkSize){

}


BaseIOStream::~BaseIOStream() {

}

void BaseIOStream::readUntil(std::string delimiter, ReadCallbackType callback) {
    ASSERT(!_readCallback, "Already reading");
    _readDelimiter = std::move(delimiter);
    _readCallback = std::move(callback);
    checkClosed();
    asyncRead();
}

void BaseIOStream::readBytes(size_t numBytes, ReadCallbackType callback) {
    ASSERT(!_readCallback, "Already reading");
    if (numBytes == 0) {
        BufferType buff;
        callback(buff);
        return;
    }
    _readBytes = numBytes;
    _readCallback = std::move(callback);
    checkClosed();
    asyncRead();
}

void BaseIOStream::write(BufferType &chunk, WriteCallbackType callback) {
    checkClosed();
    bool isWriting = writing();
    size_t bufferSize = boost::asio::buffer_size(chunk);
    MessageBuffer packet(bufferSize);
    packet.write(boost::asio::buffer_cast<const void *>(chunk), bufferSize);
    _writeQueue.push(std::move(packet));
    _writeCallback = std::move(callback);
    if (!isWriting) {
        asyncWrite();
    }
}

void BaseIOStream::readHandler(const ErrorCode &error, size_t transferredBytes) {
    if (error) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        if (error != boost::asio::error::operation_aborted) {
            if (error != boost::asio::error::eof) {
                Log::warn("Read error %d :%s", error.value(), error.message());
            }
            close();
            if (error != boost::asio::error::eof) {
                throw boost::system::system_error(error);
            }
        }
    }
    _readBuffer.writeCompleted(transferredBytes);
    if (_readBuffer.getBufferSize() > _maxBufferSize) {
        _readCallback = nullptr;
        Log::error("Reached maximum read buffer size");
        close();
        ThrowException(IOError, "Reached maximum read buffer size");
    }
    if (_readBytes > 0) {
        if (_readBuffer.getActiveSize() >= _readBytes) {
            ReadCallbackType callback = std::move(_readCallback);
            size_t readBytes = _readBytes;
            _readBytes = 0;
            BufferType buffer(_readBuffer.getReadPointer(), readBytes);
            callback(buffer);
            _readBuffer.readCompleted(readBytes);
        }
    } else if (!_readDelimiter.empty()){
        const char *loc = strnstr(_readBuffer.getReadPointer(), _readBuffer.getActiveSize(), _readDelimiter.c_str());
        if (loc) {
            size_t readBytes = loc - _readBuffer.getReadPointer() + _readDelimiter.size();
            ReadCallbackType callback = std::move(_readCallback);
            _readDelimiter.clear();
            BufferType buffer(_readBuffer.getReadPointer(), readBytes);
            callback(buffer);
            _readBuffer.readCompleted(readBytes);
        }
    }
    if (closed()) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        return;
    }
    if (_readCallback) {
        asyncRead();
    }
}

void BaseIOStream::writeHandler(const ErrorCode &error, size_t transferredBytes) {
    if (error) {
        if (_writeCallback) {
            _writeCallback = nullptr;
        }
        if (error != boost::asio::error::operation_aborted) {
            Log::warn("Write error %d :%s", error.value(), error.message());
            close();
        }
        return;
    }
    _writeQueue.front().readCompleted(transferredBytes);
    if (!_writeQueue.front().getActiveSize()) {
        _writeQueue.pop();
    }
    if (_writeQueue.empty() && _writeCallback) {
        WriteCallbackType callback = std::move(_writeCallback);
        callback();
    }
    if (closed()) {
        if (_writeCallback) {
            _writeCallback = nullptr;
        }
        return;
    }
    if (!_writeQueue.empty()) {
        asyncWrite();
    }
}

void BaseIOStream::closeHandler(const ErrorCode &error) {
    if (_closeCallback) {
        CloseCallbackType callback = std::move(_closeCallback);
        callback();
    }
    if (error) {
        Log::warn("Close error %d :%s", error.value(), error.message());
        throw boost::system::system_error(error);
    }
}

IOStream::IOStream(SocketType &&socket,
                   IOLoop *ioloop,
                   size_t maxBufferSize,
                   size_t readChunkSize)
        : BaseIOStream(std::move(socket), ioloop, maxBufferSize, readChunkSize) {
#ifndef NDEBUG
    sWatcher->inc(SYS_IOSTREAM_COUNT);
#endif
}

IOStream::~IOStream() {
#ifndef NDEBUG
    sWatcher->dec(SYS_IOSTREAM_COUNT);
#endif
}

void IOStream::asyncRead() {
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _socket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                            std::bind(&BaseIOStream::readHandler, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
}

void IOStream::asyncWrite() {
    MessageBuffer &buffer = _writeQueue.front();
    _socket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                             std::bind(&BaseIOStream::writeHandler, shared_from_this(), std::placeholders::_1,
                                       std::placeholders::_2));
}

void IOStream::asyncClose() {
    ErrorCode error;
    _socket.close(error);
    closeHandler(error);
}

SSLIOStream::SSLIOStream(SocketType &&socket,
                         SSLOption &sslOption,
                         IOLoop *ioloop,
                         size_t maxBufferSize,
                         size_t readChunkSize)
        : BaseIOStream(std::move(socket), ioloop, maxBufferSize, readChunkSize)
        , _sslSocket(_socket, sslOption.getContext()) {
#ifndef NDEBUG
    sWatcher->inc(SYS_SSLIOSTREAM_COUNT);
#endif
}

SSLIOStream::~SSLIOStream() {
#ifndef NDEBUG
    sWatcher->dec(SYS_SSLIOSTREAM_COUNT);
#endif
}

void SSLIOStream::asyncRead() {
    if (!_handshaked) {
        ErrorCode error;
        _sslSocket.handshake(boost::asio::ssl::stream_base::server, error);
        if (error) {
            Log::warn("SSL error %d :%s", error.value(), error.message());
            close();
            throw boost::system::system_error(error);
        } else {
            _handshaked = true;
        }
    }
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _sslSocket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                               std::bind(&BaseIOStream::readHandler, shared_from_this(), std::placeholders::_1,
                                         std::placeholders::_2));
}

void SSLIOStream::asyncWrite() {
    if (!_handshaked) {
        ErrorCode error;
        _sslSocket.handshake(boost::asio::ssl::stream_base::server, error);
        if (error) {
            Log::warn("SSL error %d :%s", error.value(), error.message());
            close();
            throw boost::system::system_error(error);
        } else {
            _handshaked = true;
        }
    }
    MessageBuffer& buffer = _writeQueue.front();
    _sslSocket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                                std::bind(&BaseIOStream::writeHandler, shared_from_this(), std::placeholders::_1,
                                          std::placeholders::_2));
}

void SSLIOStream::asyncClose() {
    ErrorCode error;
    if (_handshaked) {
        _sslSocket.shutdown(error);
        if (error) {
            Log::warn("SSL error %d :%s", error.value(), error.message());
        }
    }
    _socket.close(error);
    closeHandler(error);
}
