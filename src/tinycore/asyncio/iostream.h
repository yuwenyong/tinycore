//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_IOSTREAM_H
#define TINYCORE_IOSTREAM_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/logic/tribool.hpp>
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
    typedef std::shared_ptr<SSLOption> PtrType;

    SSLOption()
            : _serverSide(boost::indeterminate)
            , _context(boost::asio::ssl::context::sslv23) {
        boost::system::error_code ec;
        _context.set_options(boost::asio::ssl::context::no_sslv3, ec);
    }

    ~SSLOption() {

    }

    void initServerSide(const std::string &certFile) {
        _serverSide = true;
        setCertFile(certFile);
        setKeyFile(certFile);
    }

    void initServerSide(const std::string &certFile, const std::string &keyFile) {
        _serverSide = true;
        setCertFile(certFile);
        setKeyFile(keyFile);
    }

    void initServerSideWithPassword(const std::string &certFile, const std::string &password) {
        _serverSide = true;
        initServerSide(certFile);
        setPassword(password);
    }

    void initServerSideWithPassword(const std::string &certFile, const std::string &keyFile,
                                    const std::string &password) {
        _serverSide = true;
        initServerSide(certFile, keyFile);
        setPassword(password);
    }

    void initClientSide(SSLVerifyMode verifyMode = SSLVerifyMode::CERT_REQUIRED) {
        _serverSide = false;
        setVerifyMode(verifyMode);
        _context.set_default_verify_paths();
    }

    void initClientSide(const std::string &verifyFile) {
        initClientSide(SSLVerifyMode::CERT_REQUIRED, verifyFile);
    }

    void initClientSide(SSLVerifyMode verifyMode, const std::string &verifyFile) {
        _serverSide = false;
        setVerifyMode(verifyMode);
        setVerifyFile(verifyFile);
    }

    void initClientSideWithHost(const std::string &hostName) {
        initClientSide();
        setCheckHost(hostName);
    }

    void initClientSideWithHost(SSLVerifyMode verifyMode, const std::string &hostName) {
        initClientSide(verifyMode);
        setCheckHost(hostName);
    }

    void initClientSideWithHost(const std::string &verifyFile, const std::string &hostName) {
        initClientSide(verifyFile);
        setCheckHost(hostName);
    }

    void initClientSideWithHost(SSLVerifyMode verifyMode, const std::string &verifyFile, const std::string &hostName) {
        initClientSide(verifyMode, verifyFile);
        setCheckHost(hostName);
    }

    bool isClientSide() const {
        return !_serverSide ? true : false;
    }

    bool isServerSide() const {
        return _serverSide ? true: false;
    }

    SSLContextType &getContext() {
        return _context;
    }

    static std::shared_ptr<SSLOption>  createServerSide(const std::string &certFile) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initServerSide(certFile);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createServerSide(const std::string &certFile, const std::string &keyFile) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initServerSide(certFile, keyFile);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createServerSideWithPassword(const std::string &certFile,
                                                                    const std::string &password) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initServerSideWithPassword(certFile, password);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createServerSideWithPassword(const std::string &certFile,
                                                                    const std::string &keyFile,
                                                                    const std::string &password) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initServerSideWithPassword(certFile, keyFile, password);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSide(SSLVerifyMode verifyMode = SSLVerifyMode::CERT_REQUIRED) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSide(verifyMode);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSide(const std::string &verifyFile) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSide(verifyFile);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSide(SSLVerifyMode verifyMode, const std::string &verifyFile) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSide(verifyMode, verifyFile);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSideWithHost(const std::string &hostName) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSideWithHost(hostName);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSideWithHost(SSLVerifyMode verifyMode, const std::string &hostName) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSideWithHost(verifyMode, hostName);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSideWithHost(const std::string &verifyFile,
                                                                const std::string &hostName) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSideWithHost(verifyFile, hostName);
        return sslOption;
    }

    static std::shared_ptr<SSLOption>  createClientSideWithHost(SSLVerifyMode verifyMode, const std::string &verifyFile,
                                                                const std::string &hostName) {
        auto sslOption = std::make_shared<SSLOption>();
        sslOption->initClientSideWithHost(verifyMode, verifyFile, hostName);
        return sslOption;
    }
protected:
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

    void setCheckHost(const std::string &hostName) {
        _context.set_verify_callback(boost::asio::ssl::rfc2818_verification(hostName));
    }

    boost::tribool _serverSide;
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

    typedef std::function<void (Byte*, size_t)> ReadCallbackType;
    typedef std::function<void ()> WriteCallbackType;
    typedef std::function<void ()> CloseCallbackType;
    typedef std::function<void ()> ConnectCallbackType;

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

    void connect(const std::string &address, unsigned short port, ConnectCallbackType callback);
    void readUntil(std::string delimiter, ReadCallbackType callback);
    void readBytes(size_t numBytes, ReadCallbackType callback);
    void write(const Byte *data, size_t length, WriteCallbackType callback=nullptr);

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
        return !(_readCallback || _writeCallback || _connectCallback);
    }

    void connectHandler(const boost::system::error_code &error);
    void readHandler(const boost::system::error_code &error, size_t transferredBytes);
    void writeHandler(const boost::system::error_code &error, size_t transferredBytes);
    void closeHandler(const boost::system::error_code &error);
protected:
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
    std::string _readDelimiter;
    size_t _readBytes{0};
    bool _closed{false};
    ReadCallbackType _readCallback;
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
                IOLoop * ioloop,
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

    std::shared_ptr<SSLOption> _sslOption;
    SSLSocketType _sslSocket;
    bool _handshaking{false};
    bool _handshaked{false};
};


#endif //TINYCORE_IOSTREAM_H
