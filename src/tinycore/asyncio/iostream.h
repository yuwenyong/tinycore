//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_IOSTREAM_H
#define TINYCORE_IOSTREAM_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/common/errors.h"
#include "tinycore/utilities/messagebuffer.h"


enum class SSLVerifyMode {
    CERT_NONE,
    CERT_OPTIONAL,
    CERT_REQUIRED,
};


class SSLOption {
public:
    typedef boost::asio::ssl::context SSLContextType;

    SSLOption(bool serverSide)
            : _serverSide(serverSide)
            , _context(boost::asio::ssl::context::sslv23) {
        boost::system::error_code ec;
        _context.set_options(boost::asio::ssl::context::no_sslv3, ec);
    }

    ~SSLOption() {

    }

    void setCertFile(const std::string &certFile) {
        _context.use_certificate_chain_file(certFile);
    }

    void setKeyFile(const std::string &keyFile) {
        _context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    }

    void setPassword(const std::string &password) {
        _context.set_password_callback([password](size_t, boost::asio::ssl::context::password_purpose) {
            return password;
        });
    }

    void setVerifyMode(SSLVerifyMode verifyMode) {
        if (verifyMode == SSLVerifyMode::CERT_NONE) {
            _context.set_verify_mode(boost::asio::ssl::verify_none);
        } else if (verifyMode == SSLVerifyMode::CERT_OPTIONAL) {
            _context.set_verify_mode(boost::asio::ssl::verify_peer);
        } else {
            _context.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
        }
    }

    void setVerifyFile(const std::string &verifyFile) {
        _context.load_verify_file(verifyFile);
    }

    void setDefaultVerifyPath() {
        _context.set_default_verify_paths();
    }

    void setCheckHost(const std::string &hostName) {
        _context.set_verify_callback(boost::asio::ssl::rfc2818_verification(hostName));
    }

    bool isClientSide() const {
        return !_serverSide;
    }

    bool isServerSide() const {
        return _serverSide;
    }

    SSLContextType &context() {
        return _context;
    }

    static std::shared_ptr<SSLOption>  create(bool serverSide= false) {
        auto sslOption = std::make_shared<SSLOption>(serverSide);
        return sslOption;
    }
protected:
    bool _serverSide;
    SSLContextType _context;
};

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
                 size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                 size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~BaseIOStream();

    IOLoop* ioloop() {
        return _ioloop;
    }

    std::string getRemoteAddress() const {
        const auto & endpoint = _socket.remote_endpoint();
        return endpoint.address().to_string();
    }

    unsigned short getRemotePort() const {
        const auto & endpoint = _socket.remote_endpoint();
        return endpoint.port();
    }

    void start() {
        maybeAddErrorListener();
    }

    void clearCallbacks() {
        _readCallback = nullptr;
        _streamingCallback = nullptr;
        _writeCallback = nullptr;
        _closeCallback = nullptr;
        _connectCallback = nullptr;
    }

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
    void close();

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
            ThrowException(IOError, "Stream is closed");
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
    std::queue<MessageBuffer> _writeQueue;
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
                std::shared_ptr<SSLOption>  sslOption,
                IOLoop *ioloop,
                size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~SSLIOStream();

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
};


#endif //TINYCORE_IOSTREAM_H
