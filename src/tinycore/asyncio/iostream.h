//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_IOSTREAM_H
#define TINYCORE_IOSTREAM_H

#include "tinycore/common/common.h"
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/netutil.h"
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/common/errors.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/utilities/messagebuffer.h"


#ifdef BOOST_ASIO_HAS_IOCP
#define TC_SOCKET_USE_IOCP
#endif


DECLARE_EXCEPTION(StreamClosedError, IOError);


constexpr size_t DEFAULT_READ_CHUNK_SIZE = 4096;
constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 104857600;

class IOLoop;

class BaseIOStream: public std::enable_shared_from_this<BaseIOStream> {
public:
    typedef boost::asio::ip::tcp::socket SocketType;
    typedef boost::asio::ip::tcp::resolver ResolverType;
    typedef boost::asio::ip::tcp::endpoint EndPointType;
    typedef ResolverType::iterator ResolverIterator;

    typedef IOLoop::CallbackType CallbackType;
    typedef std::function<void (ByteArray)> ReadCallbackType;
    typedef std::function<void (ByteArray)> StreamingCallbackType;
    typedef std::function<void ()> WriteCallbackType;
    typedef std::function<void ()> CloseCallbackType;
    typedef std::function<void ()> ConnectCallbackType;

    template <typename... Args>
    class Wrapper {
    public:
        Wrapper(std::shared_ptr<BaseIOStream> stream, std::function<void(Args...)> callback)
                : _stream(std::move(stream))
                , _callback(std::move(callback)) {
        }

        Wrapper(Wrapper &&rhs)
                : _stream(std::move(rhs._stream))
                , _callback(std::move(rhs._callback)) {
            rhs._callback = nullptr;
        }

        Wrapper(const Wrapper &rhs);

        Wrapper& operator=(Wrapper &&rhs) {
            _stream = std::move(rhs._stream);
            _callback = std::move(rhs._callback);
            rhs._callback = nullptr;
        }

        Wrapper& operator=(const Wrapper &rhs);

        ~Wrapper() {
            if (_callback) {
                _stream->clearCallbacks();
            }
        }

        void operator()(Args... args) {
            std::function<void(Args...)> callback = std::move(_callback);
            _callback = nullptr;
            callback(args...);
        }
    protected:
        std::shared_ptr<BaseIOStream> _stream;
        std::function<void(Args...)> _callback;
    };

    using Wrapper1 = Wrapper<>;
    using Wrapper2 = Wrapper<const boost::system::error_code &>;
    using Wrapper3 = Wrapper<const boost::system::error_code &, size_t>;

    BaseIOStream(SocketType &&socket,
                 IOLoop * ioloop,
                 size_t maxBufferSize=0,
                 size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~BaseIOStream();

    const IOLoop* ioloop() const {
        return _ioloop;
    }

    IOLoop* ioloop() {
        return _ioloop;
    }

    std::string getRemoteAddress() const {
        auto endpoint = _socket.remote_endpoint();
        return endpoint.address().to_string();
    }

    unsigned short getRemotePort() const {
        auto endpoint = _socket.remote_endpoint();
        return endpoint.port();
    }

    void start() {
        _socket.non_blocking(true);
        maybeAddErrorListener();
    }

    virtual void clearCallbacks();
    virtual void realConnect(const std::string &address, unsigned short port) = 0;
    virtual void closeSocket() = 0;
    virtual void writeToSocket() = 0;
    virtual void readFromSocket() = 0;

    void connect(const std::string &address, unsigned short port, ConnectCallbackType callback= nullptr);
    void readUntilRegex(const std::string &regex, ReadCallbackType callback);
    void readUntil(std::string delimiter, ReadCallbackType callback);
    void readBytes(size_t numBytes, ReadCallbackType callback, StreamingCallbackType streamingCallback= nullptr);
    void readUntilClose(ReadCallbackType callback, StreamingCallbackType streamingCallback= nullptr);
    void write(const Byte *data, size_t length, WriteCallbackType callback=nullptr);
    void setCloseCallback(CloseCallbackType callback);
    void close(std::exception_ptr error= nullptr);

    bool reading() const {
        return static_cast<bool>(_readCallback);
    }

    bool writing() const {
        return !_writeQueue.empty();
    }

    bool closed() const {
        return _closing || _closed;
    }

    void setNodelay(bool value) {
        boost::asio::ip::tcp::no_delay option(value);
        _socket.set_option(option);
    }

    size_t getMaxBufferSize() const {
        return _maxBufferSize;
    }

    std::exception_ptr getError() const {
        return _error;
    }

    void onConnect(const boost::system::error_code &ec);
    void onRead(const boost::system::error_code &ec, size_t transferredBytes);
    void onWrite(const boost::system::error_code &ec, size_t transferredBytes);
    void onClose(const boost::system::error_code &ec);
protected:
    void maybeRunCloseCallback();

    void runCallback(CallbackType callback);

    void setReadCallback(ReadCallbackType &&callback) {
        ASSERT(!_readCallback, "Already reading");
        _readCallback = StackContext::wrap<ByteArray>(std::move(callback));
    }

    void tryInlineRead() {
        if (readFromBuffer()) {
            return;
        }
        checkClosed();
        if ((_state & S_READ) == S_NONE) {
            readFromSocket();
        }
    }

    size_t readToBuffer(const boost::system::error_code &ec, size_t transferredBytes);
    bool readFromBuffer();

    void checkClosed() const {
        if (closed()) {
            ThrowException(StreamClosedError, "Stream is closed");
        }
    }

    void maybeAddErrorListener();

    static constexpr int S_NONE = 0x00;
    static constexpr int S_READ = 0x01;
    static constexpr int S_WRITE = 0x04;

    SocketType _socket;
    IOLoop *_ioloop;
    size_t _maxBufferSize;
    std::exception_ptr _error;
    MessageBuffer _readBuffer;
    std::deque<MessageBuffer> _writeQueue;
    boost::optional<std::string> _readDelimiter;
    boost::optional<boost::regex> _readRegex;
    boost::optional<size_t> _readBytes;
    bool _readUntilClose{false};
    ReadCallbackType _readCallback;
    StreamingCallbackType _streamingCallback;
    WriteCallbackType _writeCallback;
    CloseCallbackType _closeCallback;
    ConnectCallbackType _connectCallback;
    bool _connecting{false};
    int _state{S_NONE};
    int _pendingCallbacks{0};
    bool _closing{false};
    bool _closed{false};
};


class IOStream: public BaseIOStream {
public:
    IOStream(SocketType &&socket,
             IOLoop *ioloop,
             size_t maxBufferSize = DEFAULT_MAX_BUFFER_SIZE,
             size_t readChunkSize = DEFAULT_READ_CHUNK_SIZE);
    virtual ~IOStream();

    void realConnect(const std::string &address, unsigned short port) override;
    void readFromSocket() override;
    void writeToSocket() override;
    void closeSocket() override;

    template <typename ...Args>
    static std::shared_ptr<IOStream> create(Args&& ...args) {
        return std::make_shared<IOStream>(std::forward<Args>(args)...);
    }
};


class SSLIOStream: public BaseIOStream {
public:
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> SSLSocketType;

    SSLIOStream(SocketType &&socket,
                std::shared_ptr<SSLOption> sslOption,
                IOLoop *ioloop,
                size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~SSLIOStream();

    void clearCallbacks() override;
    void realConnect(const std::string &address, unsigned short port) override;
    void readFromSocket() override;
    void writeToSocket() override;
    void closeSocket() override;

    template <typename ...Args>
    static std::shared_ptr<SSLIOStream> create(Args&& ...args) {
        return std::make_shared<SSLIOStream>(std::forward<Args>(args)...);
    }
protected:
    void doHandshake();
    void doRead();
    void doWrite();
    void doClose();
    void onHandshake(const boost::system::error_code &ec);
    void onShutdown(const boost::system::error_code &ec);

    std::shared_ptr<SSLOption> _sslOption;
    SSLSocketType _sslSocket;
    bool _sslAccepting{false};
    bool _sslAccepted{false};
    ConnectCallbackType _sslConnectCallback;
};


#endif //TINYCORE_IOSTREAM_H
