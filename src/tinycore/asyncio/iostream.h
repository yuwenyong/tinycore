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
#include "tinycore/common/errors.h"
#include "tinycore/utilities/messagebuffer.h"
#include "ioloop.h"


enum class SSLVerifyMode {
    CERT_NONE,
    CERT_OPTIONAL,
    CERT_REQUIRED,
};


class SSLOption {
public:
    typedef boost::asio::ssl::context SSLContextType;

    SSLOption(bool serverSide)
            : _serverSide(true)
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

    typedef IOLoop::CallbackType CallbackType;
    typedef std::function<void (ByteArray)> ReadCallbackType;
    typedef std::function<void (ByteArray)> StreamingCallbackType;
    typedef std::function<void ()> WriteCallbackType;
    typedef std::function<void ()> CloseCallbackType;
    typedef std::function<void ()> ConnectCallbackType;

    BaseIOStream(SocketType &&socket,
                 IOLoop * ioloop,
                 size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE,
                 size_t readChunkSize=DEFAULT_READ_CHUNK_SIZE);
    virtual ~BaseIOStream();

    IOLoop* ioloop() {
        return _ioloop;
    }

    std::string getRemoteAddress() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.address().to_string();
    }

    unsigned short getRemotePort() const {
        const auto & end_point = _socket.remote_endpoint();
        return end_point.port();
    }

    void start() {
        asyncRead();
    }

    void connect(const std::string &address, unsigned short port, ConnectCallbackType callback);
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

    void onConnect(const boost::system::error_code &error);
    void onRead(const boost::system::error_code &error, size_t transferredBytes);
    void onWrite(const boost::system::error_code &error, size_t transferredBytes);
    void onClose(const boost::system::error_code &error);
protected:
    void runCallback(CallbackType callback);
    virtual void asyncConnect(const std::string &address, unsigned short port) = 0;
    virtual void asyncRead() = 0;
    virtual void asyncWrite() = 0;
    virtual void asyncClose() = 0;

    size_t readToBuffer(const boost::system::error_code &error, size_t transferredBytes);
    bool readFromBuffer();

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
    boost::optional<std::string> _readDelimiter;
    boost::optional<boost::regex> _readRegex;
    boost::optional<size_t> _readBytes;
    bool _readUntilClose{false};
    bool _closed{false};
    ReadCallbackType _readCallback;
    StreamingCallbackType _streamingCallback;
    WriteCallbackType _writeCallback;
    CloseCallbackType _closeCallback;
    ConnectCallbackType _connectCallback;
    bool _connecting{false};
};


class IOStream: public BaseIOStream {
public:
    IOStream(SocketType &&socket,
             IOLoop *ioloop,
             size_t maxBufferSize = DEFAULT_MAX_BUFFER_SIZE,
             size_t readChunkSize = DEFAULT_READ_CHUNK_SIZE);
    virtual ~IOStream();

    template <typename ...Args>
    static std::shared_ptr<IOStream> create(Args&& ...args) {
        return std::make_shared<IOStream>(std::forward<Args>(args)...);
    }
protected:
    void asyncConnect(const std::string &address, unsigned short port) override;
    void asyncRead() override;
    void asyncWrite() override;
    void asyncClose() override;
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

    template <typename ...Args>
    static std::shared_ptr<SSLIOStream> create(Args&& ...args) {
        return std::make_shared<SSLIOStream>(std::forward<Args>(args)...);
    }
protected:
    void asyncConnect(const std::string &address, unsigned short port) override;
    void asyncRead() override;
    void asyncWrite() override;
    void asyncClose() override;
    void doHandshake();
    void doRead();
    void doWrite();
    void doClose();
    void onHandshake(const boost::system::error_code &error);
    void onShutdown(const boost::system::error_code &error);


    std::shared_ptr<SSLOption> _sslOption;
    SSLSocketType _sslSocket;
    bool _handshaking{false};
    bool _handshaked{false};
};


#endif //TINYCORE_IOSTREAM_H
