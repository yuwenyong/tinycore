//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/httpserver.h"
#include "tinycore/asyncio/ioloop.h"
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


void BaseIOStream::connect(const std::string &address, unsigned short port, ConnectCallbackType callback) {
    _connecting = true;
    _connectCallback = std::move(callback);
    asyncConnect(address, port);
}

void BaseIOStream::readUntil(std::string delimiter, ReadCallbackType callback) {
    ASSERT(!_readCallback, "Already reading");
    _readDelimiter = std::move(delimiter);
    _readCallback = std::move(callback);
    if (readFromBuffer()) {
        return;
    }
    checkClosed();
    asyncRead();
}

void BaseIOStream::readBytes(size_t numBytes, ReadCallbackType callback) {
    ASSERT(!_readCallback, "Already reading");
    if (numBytes == 0) {
        callback(nullptr, 0);
        return;
    }
    _readBytes = numBytes;
    _readCallback = std::move(callback);
    if (readFromBuffer()) {
        return;
    }
    checkClosed();
    asyncRead();
}

void BaseIOStream::write(const Byte *data, size_t length,  WriteCallbackType callback) {
    checkClosed();
    if (length == 0) {
        return;
    }
    bool isWriting = writing();
    MessageBuffer packet(length);
    packet.write(data, length);
    _writeQueue.push(std::move(packet));
    _writeCallback = std::move(callback);
    if (!isWriting) {
        asyncWrite();
    }
}

void BaseIOStream::connectHandler(const boost::system::error_code &error) {
    if (error) {
        if (_connectCallback) {
            _connectCallback = nullptr;
        }
        if (error != boost::asio::error::operation_aborted) {
            Log::warn("Connect error %d :%s", error.value(), error.message());
        }
        close();
    } else {
        if (_connectCallback) {
            ConnectCallbackType callback = std::move(_connectCallback);
            try {
                callback();
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
        }
    }
    _connecting = false;
}

void BaseIOStream::readHandler(const boost::system::error_code &error, size_t transferredBytes) {
    try {
        if (readToBuffer(error, transferredBytes) == 0) {
            return;
        }
    } catch (std::exception &e) {
        close();
        return;
    }
    if (readFromBuffer()) {
        return;
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

void BaseIOStream::writeHandler(const boost::system::error_code &error, size_t transferredBytes) {
    if (error) {
        if (_writeCallback) {
            _writeCallback = nullptr;
        }
        if (error != boost::asio::error::operation_aborted) {
            Log::warn("Write error %d :%s", error.value(), error.message());
        }
        close();
        return;
    }
    _writeQueue.front().readCompleted(transferredBytes);
    if (!_writeQueue.front().getActiveSize()) {
        _writeQueue.pop();
    }
    if (_writeQueue.empty() && _writeCallback) {
        WriteCallbackType callback = std::move(_writeCallback);
        try {
            callback();
        } catch (std::exception &e) {
            Log::error("Uncaught exception (%s), closing connection.", e.what());
            close();
            throw;
        }
        return;
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

void BaseIOStream::closeHandler(const boost::system::error_code &error) {
    if (_closeCallback) {
        CloseCallbackType callback = std::move(_closeCallback);
        try {
            callback();
        } catch (std::exception &e) {
            Log::error("Uncaught exception (%s), closing connection.", e.what());
            throw;
        }
    }
    if (error) {
        Log::warn("Close error %d :%s", error.value(), error.message());
    }
}

size_t BaseIOStream::readToBuffer(const boost::system::error_code &error, size_t transferredBytes) {
    if (error) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        if (error != boost::asio::error::operation_aborted) {
            if (error != boost::asio::error::eof) {
                Log::warn("Read error %d :%s", error.value(), error.message());
            }
            close();
//            if (error != boost::asio::error::eof) {
//                throw boost::system::system_error(error);
//            }
            return 0;
        }
    }
    _readBuffer.writeCompleted(transferredBytes);
    if (_readBuffer.getBufferSize() > _maxBufferSize) {
        _readCallback = nullptr;
        Log::error("Reached maximum read buffer size");
        close();
        ThrowException(IOError, "Reached maximum read buffer size");
    }
    return transferredBytes;
}

bool BaseIOStream::readFromBuffer() {
    if (_readBytes > 0) {
        if (_readBuffer.getActiveSize() >= _readBytes) {
            size_t readBytes = _readBytes;
            ReadCallbackType callback = std::move(_readCallback);
            _readBytes = 0;
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + readBytes);
            _readBuffer.readCompleted(readBytes);
            try {
                callback(data.data(), data.size());
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
            return true;
        }
    } else if (!_readDelimiter.empty()){
        const char *loc = StrNStr((const char *)_readBuffer.getReadPointer(), _readBuffer.getActiveSize(),
                                  _readDelimiter.c_str());
        if (loc) {
            size_t readBytes = loc - (const char *)_readBuffer.getReadPointer() + _readDelimiter.size();
            ReadCallbackType callback = std::move(_readCallback);
            _readDelimiter.clear();
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + readBytes);
            _readBuffer.readCompleted(readBytes);
            try {
                callback(data.data(), data.size());
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
            return true;
        }
    }
    return false;
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

void IOStream::asyncConnect(const std::string &address, unsigned short port) {
    ResolverType resolver(_ioloop->getService());
    ResolverType::query query(address, std::to_string(port));
    boost::asio::async_connect(_socket, resolver.resolve(query),
                               std::bind(&BaseIOStream::connectHandler, shared_from_this(), std::placeholders::_1));
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
    boost::system::error_code error;
    _socket.close(error);
    closeHandler(error);
}

SSLIOStream::SSLIOStream(SocketType &&socket,
                         std::shared_ptr<SSLOption>  sslOption,
                         IOLoop *ioloop,
                         size_t maxBufferSize,
                         size_t readChunkSize)
        : BaseIOStream(std::move(socket), ioloop, maxBufferSize, readChunkSize)
        , _sslOption(std::move(sslOption))
        , _sslSocket(_socket, _sslOption->getContext()) {
#ifndef NDEBUG
    sWatcher->inc(SYS_SSLIOSTREAM_COUNT);
#endif
}

SSLIOStream::~SSLIOStream() {
#ifndef NDEBUG
    sWatcher->dec(SYS_SSLIOSTREAM_COUNT);
#endif
}

void SSLIOStream::asyncConnect(const std::string &address, unsigned short port) {
    ResolverType resolver(_ioloop->getService());
    ResolverType::query query(address, std::to_string(port));
    boost::asio::async_connect(_sslSocket.lowest_layer(), resolver.resolve(query),
                               std::bind(&BaseIOStream::connectHandler, shared_from_this(), std::placeholders::_1));
}

void SSLIOStream::asyncRead() {
    doHandshake();
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _sslSocket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                               std::bind(&BaseIOStream::readHandler, shared_from_this(), std::placeholders::_1,
                                         std::placeholders::_2));
}

void SSLIOStream::asyncWrite() {
    doHandshake();
    MessageBuffer& buffer = _writeQueue.front();
    _sslSocket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                                std::bind(&BaseIOStream::writeHandler, shared_from_this(), std::placeholders::_1,
                                          std::placeholders::_2));
}

void SSLIOStream::asyncClose() {
    boost::system::error_code error;
    if (_handshaked) {
        _sslSocket.shutdown(error);
        if (error) {
            Log::warn("SSL error %d :%s", error.value(), error.message());
        }
    }
    _socket.close(error);
    closeHandler(error);
}

void SSLIOStream::doHandshake() {
    if (!_handshaked) {
        boost::system::error_code error;
        if (_sslOption->isServerSide()) {
            _sslSocket.handshake(boost::asio::ssl::stream_base::server, error);
        } else {
            _sslSocket.handshake(boost::asio::ssl::stream_base::client, error);
        }
        if (error) {
            Log::warn("SSL error %d :%s, closing connection.", error.value(), error.message());
            close();
            throw boost::system::system_error(error);
        } else {
            _handshaked = true;
        }
    }
}