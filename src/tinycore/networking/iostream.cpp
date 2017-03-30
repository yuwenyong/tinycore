//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/networking/iostream.h"
#include "tinycore/networking/httpserver.h"
#include "tinycore/debugging/trace.h"


BaseIOStream::BaseIOStream(SocketType &&socket, IOLoop *ioloop, size_t maxBufferSize, size_t readChunkSize)
        : _socket(std::move(socket))
        , _ioloop(ioloop)
        , _maxBufferSize(maxBufferSize)
        , _readBuffer(readChunkSize){

}


BaseIOStream::~BaseIOStream() {

}

int BaseIOStream::readUntil(std::string delimiter, std::function<void(const char *, size_t)> callback) {
    ASSERT(!_readCallback);
    if (!isOpen()) {
        return -1;
    }
    _readDelimiter = std::move(delimiter);
    _readCallback = std::move(callback);
    _asyncRead();
    return 0;
}

int BaseIOStream::readBytes(size_t numBytes, std::function<void(const char *, size_t)> callback) {
    ASSERT(!_readCallback);
    if (!isOpen()) {
        return -1;
    }
    if (numBytes == 0) {
        callback("", 0);
        return 0;
    }
    _readBytes = numBytes;
    _readCallback = std::move(callback);
    _asyncRead();
    return 0;
}

int BaseIOStream::write(ConstBufferType chunk, std::function<void()> callback) {
    if (!isOpen()) {
        return -1;
    }
    bool writing = isWriting();
    size_t bufferSize = boost::asio::buffer_size(chunk);
    MessageBuffer packet(bufferSize);
    packet.write(boost::asio::buffer_cast<void *>(chunk), bufferSize);
    _writeQueue.push(std::move(packet));
    _writeCallback = callback;
    if (!writing) {
        _asyncWrite();
    }
    return 0;
}

void BaseIOStream::_readHandler(const ErrorCode &error, size_t transferredBytes) {
    if (error) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        close();
        return;
    }
    _readBuffer.writeCompleted(transferredBytes);
    if (_readBuffer.getBufferSize() > _maxBufferSize) {
        //TODOLOG
        if (_readCallback) {
            _readCallback = nullptr;
        }
        close();
        return;
    }
    if (_readBytes > 0) {
        if (_readBuffer.getActiveSize() >= _readBytes) {
            std::function<void (const char*, size_t)> callback;
            size_t readBytes = _readBytes;
            callback.swap(_readCallback);
            _readBytes = 0;
            callback(_readBuffer.getReadPointer(), readBytes);
            _readBuffer.readCompleted(readBytes);
        }
    } else if (!_readDelimiter.empty()){
        const char *loc = strnstr(_readBuffer.getReadPointer(), _readBuffer.getActiveSize(), _readDelimiter.c_str());
        if (loc) {
            size_t readBytes = loc - _readBuffer.getReadPointer() + _readDelimiter.size();
            std::function<void (const char*, size_t)> callback;
            callback.swap(_readCallback);
            _readDelimiter.clear();
            callback(_readBuffer.getReadPointer(), readBytes);
            _readBuffer.readCompleted(readBytes);
        }
    }
    if (!isOpen()) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        if (!_closed) {
            close();
        }
        return;
    }
    if (_readCallback) {
        _asyncRead();
    }
}

void BaseIOStream::_writeHandler(const ErrorCode &error, size_t transferredBytes) {
    if (error) {
        if (_writeCallback) {
            _writeCallback = nullptr;
        }
        close();
        return;
    }
    _writeQueue.front().readCompleted(transferredBytes);
    if (!_writeQueue.front().getActiveSize()) {
        _writeQueue.pop();
    }
    if (_writeQueue.empty() && _writeCallback) {
        std::function<void ()> callback;
        callback.swap(_writeCallback);
        callback();
    }
    if (!isOpen()) {
        if (_writeCallback) {
            _writeCallback = nullptr;
        }
        if (!_closed) {
            close();
        }
        return;
    }
    if (!_writeQueue.empty()) {
        _asyncWrite();
    }
}

void BaseIOStream::_closeHandler(const ErrorCode &error) {
    if (error) {

    }
    if (_closeCallback) {
        std::function<void ()> callback;
        callback.swap(_closeCallback);
        callback();
    }
}

IOStream::~IOStream() {

}

void IOStream::_asyncRead() {
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _socket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                            std::bind(&BaseIOStream::_readHandler, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
}

void IOStream::_asyncWrite() {
    MessageBuffer &buffer = _writeQueue.front();
    _socket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                             std::bind(&BaseIOStream::_writeHandler, shared_from_this(), std::placeholders::_1,
                                       std::placeholders::_2));
}

void IOStream::_asyncClose() {
    ErrorCode error;
    _socket.close(error);
    _closeHandler(error);
}

SSLIOStream::SSLIOStream(SocketType &&socket,
                         SSLOption &sslOption,
                         IOLoop *ioloop,
                         size_t maxBufferSize,
                         size_t readChunkSize)
        : BaseIOStream(socket, ioloop, maxBufferSize, readChunkSize)
        , _sslSocket(_socket, sslOption.getContext()) {

}

SSLIOStream::~SSLIOStream() {

}

void SSLIOStream::_asyncRead() {
    if (!_handshaked) {
        ErrorCode error;
        _sslSocket.handshake(boost::asio::ssl::stream_base::server, error);
        if (error) {
            close();
            return;
        } else {
            _handshaked = true;
        }
    }
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _sslSocket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                               std::bind(&BaseIOStream::_readHandler, shared_from_this(), std::placeholders::_1,
                                         std::placeholders::_2));
}

void SSLIOStream::_asyncWrite() {
    if (!_handshaked) {
        ErrorCode error;
        _sslSocket.handshake(boost::asio::ssl::stream_base::server, error);
        if (error) {
            close();
            return;
        } else {
            _handshaked = true;
        }
    }
    MessageBuffer& buffer = _writeQueue.front();
    _sslSocket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                                std::bind(&BaseIOStream::_writeHandler, shared_from_this(), std::placeholders::_1,
                                          std::placeholders::_2));
}

void SSLIOStream::_asyncClose() {
    ErrorCode error;
    if (_handshaked) {
        _sslSocket.shutdown(error);
        if (error) {

        }
    }
    _socket.close(error);
    _closeHandler(error);
}
