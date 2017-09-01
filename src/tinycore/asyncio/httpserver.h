//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_HTTPSERVER_H
#define TINYCORE_HTTPSERVER_H

#include "tinycore/common/common.h"
#include <chrono>
#include "tinycore/asyncio/httputil.h"
#include "tinycore/asyncio/tcpserver.h"
#include "tinycore/httputils/cookie.h"
#include "tinycore/httputils/urlparse.h"


class HTTPServerRequest;

class HTTPServer: public TCPServer {
public:
    typedef std::function<void(std::shared_ptr<HTTPServerRequest>)> RequestCallbackType;

    HTTPServer(const HTTPServer &) = delete;

    HTTPServer &operator=(const HTTPServer &) = delete;

    HTTPServer(RequestCallbackType requestCallback,
               bool noKeepAlive = false,
               IOLoop *ioloop = nullptr,
               bool xheaders = false,
               const std::string &protocol = "",
               std::shared_ptr<SSLOption> sslOption = nullptr,
               size_t maxBufferSize=0);

    virtual ~HTTPServer();

    void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override;
protected:
    RequestCallbackType _requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    std::string _protocol;
};


class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
public:
    typedef HTTPServer::RequestCallbackType RequestCallbackType;
    typedef std::function<void ()> WriteCallbackType;
    typedef BaseIOStream::CloseCallbackType CloseCallbackType;
    typedef std::function<void (ByteArray)> HeaderCallbackType;

    class CloseCallbackWrapper {
    public:
        explicit CloseCallbackWrapper(std::shared_ptr<HTTPConnection> connection)
                : _connection(std::move(connection))
                , _needClear(true) {

        }

        CloseCallbackWrapper(CloseCallbackWrapper &&rhs) noexcept
                : _connection(std::move(rhs._connection))
                , _needClear(rhs._needClear) {
            rhs._needClear = false;
        }

        CloseCallbackWrapper& operator=(CloseCallbackWrapper &&rhs) noexcept {
            _connection = std::move(rhs._connection);
            _needClear = rhs._needClear;
            rhs._needClear = false;
            return *this;
        }

        ~CloseCallbackWrapper() {
            if (_needClear) {
                _connection->_closeCallback = nullptr;
            }
        }

        void operator()() {
            _needClear = false;
            _connection->onConnectionClose();
        }
    protected:
        std::shared_ptr<HTTPConnection> _connection;
        bool _needClear;
    };

    class WriteCallbackWrapper {
    public:
        explicit WriteCallbackWrapper(std::shared_ptr<HTTPConnection> connection)
                : _connection(std::move(connection))
                , _needClear(true) {

        }

        WriteCallbackWrapper(WriteCallbackWrapper &&rhs) noexcept
                : _connection(std::move(rhs._connection))
                , _needClear(rhs._needClear) {
            rhs._needClear = false;
        }

        WriteCallbackWrapper& operator=(WriteCallbackWrapper &&rhs) noexcept {
            _connection = std::move(rhs._connection);
            _needClear = rhs._needClear;
            rhs._needClear = false;
            return *this;
        }

        ~WriteCallbackWrapper() {
            if (_needClear) {
                _connection->_writeCallback = nullptr;
            }
        }

        void operator()() {
            _needClear = false;
            _connection->onWriteComplete();
        }
    protected:
        std::shared_ptr<HTTPConnection> _connection;
        bool _needClear;
    };

    HTTPConnection(const HTTPConnection &) = delete;

    HTTPConnection &operator=(const HTTPConnection &) = delete;

    HTTPConnection(std::shared_ptr<BaseIOStream> stream, std::string address,
                   RequestCallbackType &requestCallback,
                   bool noKeepAlive = false,
                   bool xheaders = false,
                   const std::string &protocol="");

    ~HTTPConnection();

    void clearCallbacks() {
        _writeCallback = nullptr;
        _closeCallback = nullptr;
    }

    void setCloseCallback(CloseCallbackType callback) {
        _closeCallback = StackContext::wrap(std::move(callback));
    }

    void close() {
        _stream->close();
        clearRequestState();
    }

    void start();

    void write(const Byte *chunk, size_t length, WriteCallbackType callback= nullptr);

    void finish();

    bool getNoKeepAlive() const {
        return _noKeepAlive;
    }

    bool getXHeaders() const {
        return _xheaders;
    }

    std::shared_ptr<BaseIOStream> getStream() const {
        return _stream;
    }

    std::shared_ptr<HTTPServerRequest> getRequest() const {
        return _request;
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPConnection> create(Args&& ...args) {
        return std::make_shared<HTTPConnection>(std::forward<Args>(args)...);
    }
protected:
    void clearRequestState() {
        _request.reset();
        _requestFinished = false;
        clearCallbacks();
    }

    void onConnectionClose();

    void onWriteComplete();

    void finishRequest();

    void onHeaders(ByteArray data);

    void onRequestBody(ByteArray data);

    std::shared_ptr<BaseIOStream> _stream;
    std::string _address;
    RequestCallbackType &_requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    std::string _protocol;
    std::shared_ptr<HTTPServerRequest> _request;
    std::weak_ptr<HTTPServerRequest> _requestObserver;
    bool _requestFinished{false};
    WriteCallbackType _writeCallback;
    CloseCallbackType _closeCallback;
    HeaderCallbackType _headerCallback;
};


class HTTPServerRequest {
public:
    typedef HTTPConnection::WriteCallbackType WriteCallbackType;
    typedef boost::optional<SimpleCookie> CookiesType;

    HTTPServerRequest(const HTTPServerRequest &) = delete;

    HTTPServerRequest &operator=(const HTTPServerRequest &) = delete;

    HTTPServerRequest(std::shared_ptr<HTTPConnection> connection,
                      std::string method,
                      std::string uri,
                      std::string version = "HTTP/1.0",
                      std::unique_ptr<HTTPHeaders> &&headers = nullptr,
                      std::string body = {},
                      std::string remoteIp = {},
                      std::string protocol = {},
                      std::string host = {},
                      HTTPFileListMap files = {});

    ~HTTPServerRequest();

    bool supportsHTTP11() const {
        return _version == "HTTP/1.1";
    }

    const SimpleCookie& cookies() const;

    void write(const Byte *chunk, size_t length, WriteCallbackType callback= nullptr);

    void write(const ByteArray &chunk, WriteCallbackType callback= nullptr) {
        write(chunk.data(), chunk.size(), std::move(callback));
    }

    void write(const char *chunk, WriteCallbackType callback= nullptr) {
        write((const Byte *)chunk, strlen(chunk), std::move(callback));
    }

    void write(const std::string &chunk, WriteCallbackType callback= nullptr) {
        write((const Byte *)chunk.data(), chunk.length(), std::move(callback));
    }

    void finish();

    std::string fullURL() const {
        return _protocol + "://" + _host + _uri;
    }

    double requestTime() const;

    const HTTPHeaders* getHTTPHeaders() const {
        return _headers.get();
    }

    const std::string& getMethod() const {
        return _method;
    }

    const std::string& getURI() const {
        return _uri;
    }

    const std::string& getVersion() const {
        return _version;
    }

    void setBody(std::string body) {
        _body = std::move(body);
    }

    const std::string& getBody() const {
        return _body;
    }

    const std::string& getRemoteIp() const {
        return _remoteIp;
    }

    const std::string& getProtocol() const {
        return _protocol;
    }

    const std::string& getHost() const {
        return _host;
    }

    const HTTPFileListMap& getFiles() const {
        return _files;
    }

    std::shared_ptr<HTTPConnection> getConnection() {
        return _connection;
    }

    void setConnection(std::shared_ptr<HTTPConnection> connection) {
        _connection = std::move(connection);
    }

    const std::string& getPath() const {
        return _path;
    }

    const std::string& getQuery() const {
        return _query;
    }

    QueryArgListMap& arguments() {
        return _arguments;
    }

    const QueryArgListMap& getArguments() const {
        return _arguments;
    }

    void addArgument(const std::string &name, std::string value) {
        _arguments[name].emplace_back(std::move(value));
    }

    void addArguments(const std::string &name, StringVector values) {
        for (auto &value: values) {
            addArgument(name, std::move(value));
        }
    }

    QueryArgListMap& queryArguments() {
        return _queryArguments;
    }

    const QueryArgListMap& getQueryArguments() const {
        return _queryArguments;
    }

    QueryArgListMap& bodyArguments() {
        return _bodyArguments;
    }

    const QueryArgListMap& getBodyArguments() const {
        return _bodyArguments;
    }

    HTTPFileListMap& files() {
        return _files;
    }

    void addFile(const std::string &name, HTTPFile file) {
        _files[name].emplace_back(file);
    }

    std::string dump() const;

    template <typename ...Args>
    static std::shared_ptr<HTTPServerRequest> create(Args&& ...args) {
        return std::make_shared<HTTPServerRequest>(std::forward<Args>(args)...);
    }
protected:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::unique_ptr<HTTPHeaders> _headers;
    std::string _body;
    std::string _remoteIp;
    std::string _protocol;
    std::string _host;
    HTTPFileListMap _files;
    std::shared_ptr<HTTPConnection> _connection;
    Timestamp _startTime;
    Timestamp _finishTime;
    std::string _path;
    std::string _query;
    QueryArgListMap _arguments;
    QueryArgListMap _queryArguments;
    QueryArgListMap _bodyArguments;
    mutable CookiesType _cookies;
};


#endif //TINYCORE_HTTPSERVER_H
