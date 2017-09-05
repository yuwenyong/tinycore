//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/httpserver.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/logutil.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"


BaseIOStream::BaseIOStream(SocketType &&socket, IOLoop *ioloop, size_t maxBufferSize, size_t readChunkSize,
                           size_t maxWriteBufferSize)
        : _socket(std::move(socket))
        , _ioloop(ioloop ? ioloop : IOLoop::current())
        , _maxBufferSize(maxBufferSize ? maxBufferSize : DEFAULT_MAX_BUFFER_SIZE)
        , _maxWriteBufferSize(maxWriteBufferSize)
        , _readBuffer(readChunkSize ? readChunkSize : DEFAULT_READ_CHUNK_SIZE) {

}


BaseIOStream::~BaseIOStream() {

}

void BaseIOStream::clearCallbacks() {
    _readCallback = nullptr;
    _streamingCallback = nullptr;
    _writeCallback = nullptr;
    _closeCallback = nullptr;
    _connectCallback = nullptr;
}

void BaseIOStream::connect(const std::string &address, unsigned short port, ConnectCallbackType callback) {
    _connecting = true;
    _connectCallback = StackContext::wrap(std::move(callback));
    realConnect(address, port);
}

void BaseIOStream::readUntilRegex(const std::string &regex, ReadCallbackType callback, size_t maxBytes) {
    setReadCallback(std::move(callback));
    _readRegex = boost::regex(regex);
    _readMaxBytes = maxBytes;
    try {
        tryInlineRead();
    } catch (UnsatisfiableReadError &e) {
        LOG_INFO(gGenLog, "Unsatisfiable read, closing connection");
        close(std::make_exception_ptr(e));
        throw;
    }
}

void BaseIOStream::readUntil(std::string delimiter, ReadCallbackType callback, size_t maxBytes) {
    setReadCallback(std::move(callback));
    _readDelimiter = std::move(delimiter);
    _readMaxBytes = maxBytes;
    try {
        tryInlineRead();
    } catch (UnsatisfiableReadError &e) {
        LOG_INFO(gGenLog, "Unsatisfiable read, closing connection");
        close(std::make_exception_ptr(e));
        throw;
    }
}

void BaseIOStream::readBytes(size_t numBytes, ReadCallbackType callback, StreamingCallbackType streamingCallback,
                             bool partial) {
    setReadCallback(std::move(callback));
    _readBytes = numBytes;
    _readPartial = partial;
    _streamingCallback = StackContext::wrap<ByteArray>(std::move(streamingCallback));
    tryInlineRead();
}

void BaseIOStream::readUntilClose(ReadCallbackType callback, StreamingCallbackType streamingCallback) {
    setReadCallback(std::move(callback));
    _streamingCallback = StackContext::wrap<ByteArray>(std::move(streamingCallback));
    if (closed()) {
        if (_streamingCallback) {
            runReadCallback(_readBuffer.getActiveSize(), true);
        }
        runReadCallback(_readBuffer.getActiveSize(), false);
        _streamingCallback = nullptr;
        _readCallback = nullptr;
        return;
    }
    _readUntilClose = true;
    tryInlineRead();
}

void BaseIOStream::write(const Byte *data, size_t length,  WriteCallbackType callback) {
    checkClosed();
    if (length) {
        if (_maxWriteBufferSize && _writeBufferSize + length > _maxWriteBufferSize) {
            ThrowException(StreamBufferFullError, "Reached maximum read buffer size");
        }
        constexpr size_t WRITE_BUFFER_CHUNK_SIZE = 128 * 1024;
        if (length > WRITE_BUFFER_CHUNK_SIZE) {
            size_t chunkSize;
            for (size_t i = 0; i < length; i += chunkSize) {
                chunkSize = std::min(WRITE_BUFFER_CHUNK_SIZE, length - i);
                MessageBuffer packet(chunkSize);
                packet.write(data + i, chunkSize);
                _writeQueue.push_back(std::move(packet));
            }
        } else {
            MessageBuffer packet(length);
            packet.write(data, length);
            _writeQueue.push_back(std::move(packet));
        }
        _writeBufferSize += length;
    }
    _writeCallback = StackContext::wrap(std::move(callback));
    if (!_connecting) {
        if (!_writeQueue.empty()) {
            if ((_state & S_WRITE) == S_NONE) {
                writeToSocket();
            }
        } else {
            runCallback(std::move(_writeCallback));
            _writeCallback = nullptr;
        }
    }
}

void BaseIOStream::setCloseCallback(CloseCallbackType callback) {
    _closeCallback = StackContext::wrap(std::move(callback));
    maybeAddErrorListener();
}

void BaseIOStream::close(std::exception_ptr error) {
    if (!closed()) {
        if (error) {
            _error = error;
        }
        if (_readUntilClose) {
            if (_streamingCallback && _readBuffer.getActiveSize()) {
                runReadCallback(_readBuffer.getActiveSize(), true);
            }
            _readUntilClose = false;
            runReadCallback(_readBuffer.getActiveSize(), false);
        }
        _closing = true;
        closeSocket();
    }
}

void BaseIOStream::onConnect(const boost::system::error_code &ec) {
    _state &= ~S_WRITE;
    if (ec) {
        _connectCallback = nullptr;
        if (ec != boost::asio::error::operation_aborted) {
            LOG_WARNING(gGenLog, "Connect error %d :%s", ec.value(), ec.message());
            close(std::make_exception_ptr(boost::system::system_error(ec)));
        } else {
            maybeRunCloseCallback();
        }
        return;
    } else {
        if (_connectCallback) {
            ConnectCallbackType callback(std::move(_connectCallback));
            _connectCallback = nullptr;
            runCallback(std::move(callback));
        }
    }
    _socket.non_blocking(true);
    _connecting = false;
    if (!_writeQueue.empty()) {
        writeToSocket();
    } else if (_writeCallback) {
        runCallback(std::move(_writeCallback));
        _writeCallback = nullptr;
    } else {
        maybeAddErrorListener();
    }
}

void BaseIOStream::onRead(const boost::system::error_code &ec, size_t transferredBytes) {
    _state &= ~S_READ;
    boost::optional<size_t> pos;
    try {
        pos = readToBuffer(ec, transferredBytes);
    } catch (UnsatisfiableReadError &e) {
        LOG_INFO(gGenLog, "Unsatisfiable read, closing connection");
        close(std::make_exception_ptr(e));
        return;
    } catch (std::exception &e) {
        close();
        return;
    }
    if (pos) {
        readFromBuffer(pos.get());
        return;
    }
    if (closed()) {
        return;
    }
    if (reading()) {
        readFromSocket();
    } else {
        maybeAddErrorListener();
    }
}

void BaseIOStream::onWrite(const boost::system::error_code &ec, size_t transferredBytes) {
    _state &= ~S_WRITE;
    if (ec) {
        _writeCallback = nullptr;
        if (ec != boost::asio::error::operation_aborted) {
            LOG_WARNING(gGenLog, "Write error %d :%s", ec.value(), ec.message());
            close(std::make_exception_ptr(boost::system::system_error(ec)));
        } else {
            maybeRunCloseCallback();
        }
        return;
    }
    if (transferredBytes > 0) {
        _writeQueue.front().readCompleted(transferredBytes);
        if (!_writeQueue.front().getActiveSize()) {
            _writeQueue.pop_front();
        }
        _writeBufferSize -= transferredBytes;
    }
    if (_writeQueue.empty() && _writeCallback) {
        WriteCallbackType callback;
        callback.swap(_writeCallback);
        runCallback(std::move(callback));
        return;
    }
    if (closed()) {
        return;
    }
    if (writing()) {
        writeToSocket();
    } else {
        maybeAddErrorListener();
    }
}

void BaseIOStream::onClose(const boost::system::error_code &ec) {
    _closing = false;
    _closed = true;
    if (ec) {
        if (!_error) {
            _error = std::make_exception_ptr(boost::system::system_error(ec));
        }
        LOG_WARNING(gGenLog, "Close error %d :%s", ec.value(), ec.message());
    }
    maybeRunCloseCallback();
}

void BaseIOStream::maybeRunCloseCallback() {
    if (_closeCallback) {
        if (_closed && _pendingCallbacks == 0) {
            CloseCallbackType callback(std::move(_closeCallback));
            _closeCallback = nullptr;
            runCallback(std::move(callback));
            clearCallbacks();
        } else {
            _readCallback = nullptr;
            _writeCallback = nullptr;
        }
        _writeQueue.clear();
    } else {
        if (_pendingCallbacks == 0) {
            clearCallbacks();
        }
    }
}

void BaseIOStream::runCallback(CallbackType callback) {
    NullContext ctx;
    ++_pendingCallbacks;
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    auto op = std::make_shared<Wrapper1>(shared_from_this(), [this, callback=std::move(callback)](){
        --_pendingCallbacks;
        try {
            callback();
        } catch (std::exception &e) {
            LOG_ERROR(gAppLog, "Uncaught exception (%s), closing connection.", e.what());
            close(std::current_exception());
            throw;
        } catch (...) {
            LOG_ERROR(gAppLog, "Unknown exception, closing connection.");
            close(std::current_exception());
        }
        maybeAddErrorListener();
    });
#else
    auto op = std::make_shared<Wrapper1>(shared_from_this(), std::bind([this](CallbackType &callback) {
        --_pendingCallbacks;
        try {
            callback();
        } catch (std::exception &e) {
            LOG_ERROR(gAppLog, "Uncaught exception (%s), closing connection.", e.what());
            close(std::current_exception());
            throw;
        } catch (...) {
            LOG_ERROR(gAppLog, "Unknown exception, closing connection.");
            close(std::current_exception());
        }
        maybeAddErrorListener();
    }, std::move(callback)));
#endif
    _ioloop->addCallback(std::bind(&Wrapper1::operator(), std::move(op)));
}

void BaseIOStream::runReadCallback(size_t size, bool streaming) {
    ReadCallbackType callback;
    if (streaming) {
        callback = _streamingCallback;
    } else {
        callback = std::move(_readCallback);
        _readCallback = nullptr;
        _streamingCallback = nullptr;
    }
    if (callback) {
        ByteArray data;
        if (size) {
            data.assign(_readBuffer.getReadPointer(), _readBuffer.getReadPointer() + size);
            _readBuffer.readCompleted(size);
        }
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        runCallback([callback=std::move(callback), data=std::move(data)](){
            callback(std::move(data));
        });
#else
        runCallback(std::bind([](ReadCallbackType &callback, ByteArray &data){
            callback(std::move(data));
        }, std::move(callback), std::move(data)));
#endif
    } else {
        maybeAddErrorListener();
    }
}

void BaseIOStream::runStreamingCallback() {
    if (_streamingCallback && _readBuffer.getActiveSize() > 0) {
        size_t bytesToConsume = _readBuffer.getActiveSize();
        if (_readBytes) {
            bytesToConsume = std::min(_readBytes.get(), bytesToConsume);
            _readBytes.get() -= bytesToConsume;
        }
        runReadCallback(bytesToConsume, true);
    }
}

boost::optional<size_t> BaseIOStream::readToBuffer(const boost::system::error_code &ec, size_t transferredBytes) {
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            if (ec != boost::asio::error::eof) {
                LOG_WARNING(gGenLog, "Read error %d :%s", ec.value(), ec.message());
            }
            close(std::make_exception_ptr(boost::system::system_error(ec)));
        } else {
            maybeRunCloseCallback();
        }
        return boost::none;
    }
    _readBuffer.writeCompleted(transferredBytes);
    if (_readBuffer.getBufferSize() > _maxBufferSize) {
        LOG_ERROR(gGenLog, "Reached maximum read buffer size");
        close();
        ThrowException(StreamBufferFullError, "Reached maximum read buffer size");
    }
    runStreamingCallback();
    return findReadPos();
}

void BaseIOStream::readFromBuffer(size_t pos) {
    _readCallback = nullptr;
    _streamingCallback = nullptr;
    _readBytes = boost::none;
    _readPartial = false;
    runReadCallback(pos, false);
}

boost::optional<size_t> BaseIOStream::findReadPos() {
    if (_readBytes && (_readBuffer.getActiveSize() >= _readBytes.get()
                       || (_readPartial && _readBuffer.getActiveSize() > 0))) {
        size_t numBytes = std::min(_readBytes.get(), _readBuffer.getActiveSize());
        return numBytes;
    } else if (_readDelimiter) {
        const char *loc = StrNStr((const char *)_readBuffer.getReadPointer(), _readBuffer.getActiveSize(),
                                  _readDelimiter->c_str());
        if (loc) {
            size_t readBytes = loc - (const char *)_readBuffer.getReadPointer() + _readDelimiter->size();
            checkMaxBytes(_readDelimiter.get(), readBytes);
            return readBytes;
        }
        checkMaxBytes(_readDelimiter.get(), _readBuffer.getActiveSize());
    } else if (_readRegex) {
        boost::cmatch m;
        if (boost::regex_search((const char *) _readBuffer.getReadPointer(),
                                (const char *) _readBuffer.getReadPointer() + _readBuffer.getActiveSize(), m,
                                _readRegex.get())) {
            size_t readBytes = (size_t)m.position((size_t) 0) + m.length();
            checkMaxBytes(_readRegex->str(), readBytes);
            return readBytes;
        }
        checkMaxBytes(_readRegex->str(), _readBuffer.getActiveSize());
    }
    return boost::none;
}

void BaseIOStream::maybeAddErrorListener() {
    if (_pendingCallbacks != 0) {
        return;
    }
    if (_state == S_NONE) {
        if (closed()) {
            maybeRunCloseCallback();
        } else if (_closeCallback) {
            readFromSocket();
        }
    }
}


IOStream::IOStream(SocketType &&socket,
                   IOLoop *ioloop,
                   size_t maxBufferSize,
                   size_t readChunkSize,
                   size_t maxWriteBufferSize)
        : BaseIOStream(std::move(socket), ioloop, maxBufferSize, readChunkSize, maxWriteBufferSize) {
#ifndef NDEBUG
    sWatcher->inc(SYS_IOSTREAM_COUNT);
#endif
}

IOStream::~IOStream() {
#ifndef NDEBUG
    sWatcher->dec(SYS_IOSTREAM_COUNT);
#endif
}

void IOStream::realConnect(const std::string &address, unsigned short port) {
    ResolverType resolver(_ioloop->getService());
    ResolverType::query query(address, std::to_string(port));
    boost::system::error_code ec;
    auto iter = resolver.resolve(query, ec);
    if (ec) {
        onConnect(ec);
        return;
    }
    auto op = std::make_shared<Wrapper2>(shared_from_this(), [this](const boost::system::error_code &ec) {
        onConnect(ec);
    });
    boost::asio::async_connect(_socket, iter, std::bind(&Wrapper2::operator(), std::move(op), std::placeholders::_1));
    _state |= S_WRITE;
}

void IOStream::readFromSocket() {
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    Wrapper3 op(shared_from_this(), [this](const boost::system::error_code &ec, size_t transferredBytes) {
        onRead(ec, transferredBytes);
    });
    _socket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                            std::move(op));
    _state |= S_READ;
}


void IOStream::writeToSocket() {
#ifndef TC_SOCKET_USE_IOCP
    size_t bytesToSend, bytesSent;
    boost::system::error_code ec;
    for(;;) {
        MessageBuffer &buffer = _writeQueue.front();
        bytesToSend = buffer.getActiveSize();
        bytesSent = _socket.write_some(boost::asio::buffer(buffer.getReadPointer(), bytesToSend), ec);
        if (ec) {
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                break;
            }
            onWrite(ec, 0);
            return;
        } else if (bytesSent == 0){
            close();
            return;
        } else if (bytesSent < bytesToSend) {
            buffer.readCompleted(bytesSent);
            _writeBufferSize -= bytesSent;
            break;
        }
        _writeQueue.pop_front();
        _writeBufferSize -= bytesSent;
        if (_writeQueue.empty()) {
            onWrite(ec, 0);
            return;
        }
    }
#endif
    MessageBuffer &buffer = _writeQueue.front();
#ifdef TC_SOCKET_USE_IOCP
    if (_writeQueue.size() > 1) {
        size_t space = 0;
        for (auto iter = std::next(_writeQueue.begin()); iter != _writeQueue.end(); ++iter) {
            space += iter->getActiveSize();
        }
        buffer.ensureFreeSpace(space);
        for (auto iter = std::next(_writeQueue.begin()); iter != _writeQueue.end(); ++iter) {
            buffer.write(iter->getReadPointer(), iter->getActiveSize());
        }
        while (_writeQueue.size() > 1) {
            _writeQueue.pop_back();
        }
    }
#endif
    Wrapper3 op(shared_from_this(), [this](const boost::system::error_code &ec, size_t transferredBytes) {
        onWrite(ec, transferredBytes);
    });
    _socket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()), std::move(op));
    _state |= S_WRITE;
}

void IOStream::closeSocket() {
    boost::system::error_code ec;
    _socket.close(ec);
    onClose(ec);
}

SSLIOStream::SSLIOStream(SocketType &&socket,
                         std::shared_ptr<SSLOption>  sslOption,
                         IOLoop *ioloop,
                         size_t maxBufferSize,
                         size_t readChunkSize,
                         size_t maxWriteBufferSize)
        : BaseIOStream(std::move(socket), ioloop, maxBufferSize, readChunkSize, maxWriteBufferSize)
        , _sslOption(std::move(sslOption))
        , _sslSocket(_socket, _sslOption->context()) {
#ifndef NDEBUG
    sWatcher->inc(SYS_SSLIOSTREAM_COUNT);
#endif
}

SSLIOStream::~SSLIOStream() {
#ifndef NDEBUG
    sWatcher->dec(SYS_SSLIOSTREAM_COUNT);
#endif
}

void SSLIOStream::clearCallbacks() {
    BaseIOStream::clearCallbacks();
    _sslConnectCallback = nullptr;
}

void SSLIOStream::realConnect(const std::string &address, unsigned short port) {
    ResolverType resolver(_ioloop->getService());
    ResolverType::query query(address, std::to_string(port));
    boost::system::error_code ec;
    auto iter = resolver.resolve(query, ec);
    if (ec) {
        onConnect(ec);
        return;
    }
    _sslConnectCallback = std::move(_connectCallback);
    _connectCallback = nullptr;
    auto op = std::make_shared<Wrapper2>(shared_from_this(), [this](const boost::system::error_code &ec) {
        onConnect(ec);
    });
    boost::asio::async_connect(_sslSocket.lowest_layer(), iter,
                               std::bind(&Wrapper2::operator(), std::move(op), std::placeholders::_1));
    _state |= S_WRITE;
}

void SSLIOStream::readFromSocket() {
    if (_sslAccepted) {
        doRead();
    } else {
        doHandshake();
    }
}

void SSLIOStream::writeToSocket() {
    if (_sslAccepted) {
        doWrite();
    } else {
        doHandshake();
    }
}

void SSLIOStream::closeSocket() {
    if (_sslAccepted && ((_state & S_WRITE) == S_NONE)) {
        auto self = std::static_pointer_cast<SSLIOStream>(shared_from_this());
        Wrapper2 op(shared_from_this(), [this](const boost::system::error_code &ec) {
            onShutdown(ec);
        });
        _sslSocket.async_shutdown(std::move(op));
        _state |= S_WRITE;
    } else {
        doClose();
    }
}

void SSLIOStream::doHandshake() {
    if (!_sslAccepted && !_sslAccepting) {
        Wrapper2 op(shared_from_this(), [this](const boost::system::error_code &ec) {
            onHandshake(ec);
        });
        if (_sslOption->isServerSide()) {
            _sslSocket.async_handshake(boost::asio::ssl::stream_base::server, std::move(op));

        } else {
            _sslSocket.async_handshake(boost::asio::ssl::stream_base::client, std::move(op));
        }
        _sslAccepting = true;
        _state |= S_WRITE;
    }
}

void SSLIOStream::doRead() {
    _readBuffer.normalize();
    _readBuffer.ensureFreeSpace();
    Wrapper3 op(shared_from_this(), [this](const boost::system::error_code &ec, size_t transferredBytes) {
        onRead(ec, transferredBytes);
    });
    _sslSocket.async_read_some(boost::asio::buffer(_readBuffer.getWritePointer(), _readBuffer.getRemainingSpace()),
                               std::move(op));
    _state |= S_READ;
}

void SSLIOStream::doWrite() {
    Wrapper3 op(shared_from_this(), [this](const boost::system::error_code &ec, size_t transferredBytes) {
        onWrite(ec, transferredBytes);
    });
    MessageBuffer& buffer = _writeQueue.front();
    if (_writeQueue.size() > 1) {
        size_t space = 0;
        for (auto iter = std::next(_writeQueue.begin()); iter != _writeQueue.end(); ++iter) {
            space += iter->getActiveSize();
        }
        buffer.ensureFreeSpace(space);
        for (auto iter = std::next(_writeQueue.begin()); iter != _writeQueue.end(); ++iter) {
            buffer.write(iter->getReadPointer(), iter->getActiveSize());
        }
        while (_writeQueue.size() > 1) {
            _writeQueue.pop_back();
        }
    }
    _sslSocket.async_write_some(boost::asio::buffer(buffer.getReadPointer(), buffer.getActiveSize()),
                                std::move(op));
    _state |= S_WRITE;
}

void SSLIOStream::doClose() {
    boost::system::error_code ec;
    _socket.close(ec);
    onClose(ec);
}

void SSLIOStream::onHandshake(const boost::system::error_code &ec) {
    _state &= ~S_WRITE;
    _sslAccepting = false;
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            LOG_WARNING(gGenLog, "SSL error %d :%s, closing connection.", ec.value(), ec.message());
            close(std::make_exception_ptr(boost::system::system_error(ec)));
        }
    } else {
        _sslAccepted = true;
        if (_sslConnectCallback) {
            ConnectCallbackType callback(std::move(_sslConnectCallback));
            _sslConnectCallback = nullptr;
            runCallback(std::move(callback));
        }
        if (writing()) {
            doWrite();
        }
        if (reading()) {
            doRead();
        }
        maybeAddErrorListener();
    }
}

void SSLIOStream::onShutdown(const boost::system::error_code &ec) {
    _state &= ~S_WRITE;
    if (ec.category() == boost::asio::error::get_ssl_category()) {
        LOG_WARNING(gGenLog, "SSL shutdown error %d :%s", ec.value(), ec.message());
    }
    doClose();
}