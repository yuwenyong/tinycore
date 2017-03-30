//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_IOSTREAM_H
#define TINYCORE_IOSTREAM_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "tinycore/utilities/messagebuffer.h"


constexpr size_t DEFAULT_READ_CHUNK_SIZE = 4096;
constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 104857600;

class IOLoop;

class BaseIOStream: public std::enable_shared_from_this<BaseIOStream> {
public:
    typedef boost::asio::ip::tcp::socket SocketType;
    typedef boost::asio::ip::tcp::resolver ResolverType;
    typedef boost::asio::ip::tcp::endpoint EndPointType;
    typedef boost::asio::mutable_buffer MutableBufferType;
    typedef boost::asio::const_buffer ConstBufferType;

    BaseIOStream(SocketType &&socket,
                 IOLoop * ioloop,
                 size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                 size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~BaseIOStream();

    const std::string & getRemoteAddress() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.address().to_string();
    }

    unsigned short getRemotePort() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.port();
    }

    int readUntil(std::string delimiter, std::function<void (const char *, size_t)> callback);
    int readBytes(size_t numBytes, std::function<void (const char *, size_t)> callback);
    int write(ConstBufferType chunk, std::function<void ()> callback= nullptr);

    int close() {
        if (_closed) {
            return -1;
        }
        _closed = true;
        _asyncClose();
        return 0;
    }

    int delayedClose() {
        _closing = true;
        return 0;
    }

    bool isOpen() const {
        return !_closed && !_closing;
    }

    bool isReading() const {
        return static_cast<bool>(_readCallback);
    }

    bool isWriting() const {
        return !_writeQueue.empty();
    }

    size_t getMaxBufferSize() const {
        return _maxBufferSize;
    }


protected:
    virtual void _asyncRead() = 0;
    virtual void _asyncWrite() = 0;
    virtual void _asyncClose() = 0;

    void _readHandler(const ErrorCode &error, size_t transferredBytes);
    void _writeHandler(const ErrorCode &error, size_t transferredBytes);
    void _closeHandler(const ErrorCode &error);

    SocketType _socket;
    IOLoop *_ioloop;
    size_t _maxBufferSize;
    MessageBuffer _readBuffer;
    std::queue<MessageBuffer> _writeQueue;
    std::string _readDelimiter;
    size_t _readBytes{0};
    bool _closed{false};
    bool _closing{false};
    std::function<void (const char*, size_t)> _readCallback;
    std::function<void ()> _writeCallback;
    std::function<void ()> _closeCallback;
};


typedef std::shared_ptr<BaseIOStream> BaseIOStreamPtr;


class IOStream: public BaseIOStream {
public:
    using BaseIOStream::BaseIOStream;
    virtual ~IOStream();

protected:
    void _asyncRead() override;
    void _asyncWrite() override;
    void _asyncClose() override;
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
    void _asyncRead() override;
    void _asyncWrite() override;
    void _asyncClose() override;

    SSLSocketType _sslSocket;
    bool _handshaking{false};
    bool _handshaked{false};
};


#endif //TINYCORE_IOSTREAM_H
