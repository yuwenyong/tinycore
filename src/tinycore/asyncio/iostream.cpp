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

void BaseIOStream::readUntilRegex(const std::string &regex, ReadCallbackType callback) {
    ASSERT(!_readCallback, "Already reading");
    _readRegex = boost::regex(regex);
    _readCallback = std::move(callback);
    if (readFromBuffer()) {
        return;
    }
    checkClosed();
    asyncRead();
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

void BaseIOStream::readBytes(size_t numBytes, ReadCallbackType callback, StreamingCallbackType streamingCallback) {
    ASSERT(!_readCallback, "Already reading");
//    if (numBytes == 0) {
//        callback(ByteArray());
//        return;
//    }
    _readBytes = numBytes;
    _readCallback = std::move(callback);
    _streamingCallback = std::move(streamingCallback);
    if (readFromBuffer()) {
        return;
    }
    checkClosed();
    asyncRead();
}

void BaseIOStream::readUntilClose(ReadCallbackType callback, StreamingCallbackType streamingCallback) {
    ASSERT(!_readCallback, "Already reading");
    if (closed()) {
        callback(_readBuffer.move());
        return;
    }
    _readUntilClose = true;
    _readCallback = std::move(callback);
    _streamingCallback = std::move(streamingCallback);
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

void BaseIOStream::close() {
    if (!closed()) {
        if (_readUntilClose) {
            ReadCallbackType callback;
            callback.swap(_readCallback);
            _readUntilClose = false;
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + _readBuffer.getActiveSize());
            _readBuffer.readCompleted(_readBuffer.getActiveSize());
            callback(std::move(data));
        }
        _closed = true;
        asyncClose();
    }
}

void BaseIOStream::onConnect(const boost::system::error_code &error) {
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
            ConnectCallbackType callback;
            callback.swap(_connectCallback);
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

void BaseIOStream::onRead(const boost::system::error_code &error, size_t transferredBytes) {
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

void BaseIOStream::onWrite(const boost::system::error_code &error, size_t transferredBytes) {
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
        WriteCallbackType callback;
        callback.swap(_writeCallback);
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

void BaseIOStream::onClose(const boost::system::error_code &error) {
    if (_closeCallback) {
        CloseCallbackType callback;
        callback.swap(_closeCallback);
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
//    fprintf(stderr,"ReadToBuffer:%d\n", (int)transferredBytes);
    if (error) {
        if (_readCallback) {
            _readCallback = nullptr;
        }
        if (_streamingCallback) {
            _streamingCallback = nullptr;
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
    if (_readBytes) {
        if (_streamingCallback && _readBuffer.getActiveSize() > 0) {
            size_t bytesToConsume = std::min(_readBytes.get(), _readBuffer.getActiveSize());
            _readBytes.get() -= bytesToConsume;
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + bytesToConsume);
            _readBuffer.readCompleted(bytesToConsume);
            _streamingCallback(std::move(data));
        }
        if (_readBuffer.getActiveSize() >= _readBytes.get()) {
            size_t numBytes = _readBytes.get();
            ReadCallbackType callback;
            callback.swap(_readCallback);
            _streamingCallback = nullptr;
            _readBytes = boost::none;
            ByteArray data;
            if (numBytes > 0) {
                data.assign(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + numBytes);
                _readBuffer.readCompleted(numBytes);
            }
            try {
//                fprintf(stderr, "Handle Read,consume:%d,left:%d\n", (int)readBytes, (int)_readBuffer.getActiveSize());
                callback(std::move(data));
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
            return true;
        }
    } else if (_readDelimiter) {
        const char *loc = StrNStr((const char *)_readBuffer.getReadPointer(), _readBuffer.getActiveSize(),
                                  _readDelimiter->c_str());
        if (loc) {
            size_t readBytes = loc - (const char *)_readBuffer.getReadPointer() + _readDelimiter->size();
            ReadCallbackType callback;
            callback.swap(_readCallback);
            _streamingCallback = nullptr;
            _readDelimiter = boost::none;
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + readBytes);
            _readBuffer.readCompleted(readBytes);
            try {
//                fprintf(stderr, "Handle ReadUtil,Consume:%d,Left:%d\n", (int)readBytes,
//                        (int)_readBuffer.getActiveSize());
                callback(std::move(data));
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
            return true;
        }
    } else if (_readRegex) {
        boost::cmatch m;
        if (boost::regex_search((const char *)_readBuffer.getReadPointer(),
                                (const char *)_readBuffer.getReadPointer() + _readBuffer.getActiveSize(), m,
                                _readRegex.get())) {
            auto readBytes = m.position((size_t)0) + m.length();
            ReadCallbackType callback;
            callback.swap(_readCallback);
            _streamingCallback = nullptr;
            _readRegex = boost::none;
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + readBytes);
            _readBuffer.readCompleted((size_t)readBytes);
            try {
                callback(std::move(data));
            } catch (std::exception &e) {
                Log::error("Uncaught exception (%s), closing connection.", e.what());
                close();
                throw;
            }
            return true;
        }
    } else if (_readUntilClose) {
        if (_streamingCallback && _readBuffer.getActiveSize() > 0) {
            ByteArray data(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + _readBuffer.getActiveSize());
            _readBuffer.readCompleted(_readBuffer.getActiveSize());
            _streamingCallback(std::move(data));
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
                               std::bind(&BaseIOStream::onConnect, shared_from_this(), std::placeholders::_1));
}

void IOStream::asyncRead() {
//    fprintf(stderr, "AsyncRead\n");
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _socket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                            std::bind(&BaseIOStream::onRead, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2));
}

void IOStream::asyncWrite() {
    MessageBuffer &buffer = _writeQueue.front();
    _socket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                             std::bind(&BaseIOStream::onWrite, shared_from_this(), std::placeholders::_1,
                                       std::placeholders::_2));
}

void IOStream::asyncClose() {
    boost::system::error_code error;
    _socket.close(error);
    onClose(error);
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
                               std::bind(&BaseIOStream::onConnect, shared_from_this(), std::placeholders::_1));
}

void SSLIOStream::asyncRead() {
    if (_handshaked) {
        doRead();
    } else {
        doHandshake();
    }
}

void SSLIOStream::asyncWrite() {
    if (_handshaked) {
        doWrite();
    } else {
        doHandshake();
    }
}

void SSLIOStream::asyncClose() {
    if (_handshaked) {
        auto self = std::static_pointer_cast<SSLIOStream>(shared_from_this());
        _sslSocket.async_shutdown(std::bind(&SSLIOStream::onShutdown, self, std::placeholders::_1));
    } else {
        doClose();
    }
}

void SSLIOStream::doHandshake() {
    if (!_handshaked && !_handshaking) {
        auto self = std::static_pointer_cast<SSLIOStream>(shared_from_this());
        if (_sslOption->isServerSide()) {

            _sslSocket.async_handshake(boost::asio::ssl::stream_base::server, std::bind(&SSLIOStream::onHandshake,
                                                                                        self, std::placeholders::_1));
        } else {
            _sslSocket.async_handshake(boost::asio::ssl::stream_base::client, std::bind(&SSLIOStream::onHandshake,
                                                                                        self, std::placeholders::_1));
        }
        _handshaking = true;
    }
}

void SSLIOStream::doRead() {
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    _sslSocket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                               std::bind(&BaseIOStream::onRead, shared_from_this(), std::placeholders::_1,
                                         std::placeholders::_2));
}

void SSLIOStream::doWrite() {
    MessageBuffer& buffer = _writeQueue.front();
    _sslSocket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                                std::bind(&BaseIOStream::onWrite, shared_from_this(), std::placeholders::_1,
                                          std::placeholders::_2));
}

void SSLIOStream::doClose() {
    boost::system::error_code error;
    _socket.close(error);
    onClose(error);
}

void SSLIOStream::onHandshake(const boost::system::error_code &error) {
    _handshaking = false;
    if (error) {
        if (error != boost::asio::error::operation_aborted) {
            Log::warn("SSL error %d :%s, closing connection.", error.value(), error.message());
        }
        close();
        throw boost::system::system_error(error);
    } else {
        _handshaked = true;
        if (writing()) {
            doWrite();
        }
        if (reading()) {
            doRead();
        }
    }
}

void SSLIOStream::onShutdown(const boost::system::error_code &error) {
    if (error.category() == boost::asio::error::get_ssl_category()) {
        Log::warn("SSL shutdown error %d :%s", error.value(), error.message());
    }
    doClose();
}