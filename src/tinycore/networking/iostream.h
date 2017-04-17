//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_IOSTREAM_H
#define TINYCORE_IOSTREAM_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/utilities/messagebuffer.h"


constexpr size_t DEFAULT_READ_CHUNK_SIZE = 4096;
constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 104857600;

class IOLoop;

class BaseIOStream: public std::enable_shared_from_this<BaseIOStream> {
public:
    typedef boost::asio::ip::tcp::socket SocketType;
    typedef boost::asio::ip::tcp::resolver ResolverType;
    typedef boost::asio::ip::tcp::endpoint EndPointType;
//    typedef boost::asio::mutable_buffer MutableBufferType;
    typedef boost::asio::const_buffer BufferType;
    typedef std::function<void (BufferType&)> ReadCallbackType;
    typedef std::function<void ()> WriteCallbackType;
    typedef std::function<void ()> CloseCallbackType;

    BaseIOStream(SocketType &&socket,
                 IOLoop * ioloop,
                 size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                 size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~BaseIOStream();

    std::string getRemoteAddress() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.address().to_string();
    }

    unsigned short getRemotePort() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.port();
    }

    void readUntil(std::string delimiter, ReadCallbackType callback);
    void readBytes(size_t numBytes, ReadCallbackType callback);
    void write(BufferType &chunk, WriteCallbackType callback=nullptr);

    void setCloseCallback(CloseCallbackType callback) {
        _closeCallback = std::move(callback);
    }

    void close() {
        if (!closed()) {
            _closed = true;
            asyncClose();
        }
    }

    bool reading() const {
        return static_cast<bool>(_readCallback);
    }

    bool writing() const {
        return !_writeQueue.empty();
    }

    bool closed() const {
        return _closed;
    }

    size_t getMaxBufferSize() const {
        return _maxBufferSize;
    }

    bool dying() const {
        return !(_readCallback || _writeCallback);
    }

    void readHandler(const ErrorCode &error, size_t transferredBytes);
    void writeHandler(const ErrorCode &error, size_t transferredBytes);
    void closeHandler(const ErrorCode &error);
protected:
    virtual void asyncRead() = 0;
    virtual void asyncWrite() = 0;
    virtual void asyncClose() = 0;

    void checkClosed() const {
        if (closed()) {
            ThrowException(IOError, "Stream is closed");
        }
    }

    SocketType _socket;
    IOLoop *_ioloop;
    size_t _maxBufferSize;
    MessageBuffer _readBuffer;
    std::queue<MessageBuffer> _writeQueue;
    std::string _readDelimiter;
    size_t _readBytes{0};
    bool _closed{false};
    ReadCallbackType _readCallback;
    WriteCallbackType _writeCallback;
    CloseCallbackType _closeCallback;
};


typedef std::shared_ptr<BaseIOStream> BaseIOStreamPtr;
typedef std::weak_ptr<BaseIOStream> BaseIOStreamWPtr;


class IOStream: public BaseIOStream {
public:
    IOStream(SocketType &&socket,
             IOLoop *ioloop,
             size_t maxBufferSize = DEFAULT_MAX_BUFFER_SIZE,
             size_t readChunkSize = DEFAULT_READ_CHUNK_SIZE);
    virtual ~IOStream();

protected:
    void asyncRead() override;
    void asyncWrite() override;
    void asyncClose() override;
};


class SSLOption;


class SSLIOStream: public BaseIOStream {
public:
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> SSLSocketType;

    SSLIOStream(SocketType &&socket,
                SSLOption &sslOption,
                IOLoop * ioloop,
                size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~SSLIOStream();

protected:
    void asyncRead() override;
    void asyncWrite() override;
    void asyncClose() override;

    SSLSocketType _sslSocket;
    bool _handshaking{false};
    bool _handshaked{false};
};


#endif //TINYCORE_IOSTREAM_H
