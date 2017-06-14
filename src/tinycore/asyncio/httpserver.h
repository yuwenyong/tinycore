//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_HTTPSERVER_H
#define TINYCORE_HTTPSERVER_H

#include "tinycore/common/common.h"
#include <chrono>
#include "tinycore/asyncio/httputil.h"
#include "tinycore/asyncio/netutil.h"
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
               std::shared_ptr<SSLOption> sslOption = nullptr);

    virtual ~HTTPServer();

    void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override;
protected:
    RequestCallbackType _requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
};


class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
public:
    typedef HTTPServer::RequestCallbackType RequestCallbackType;
    typedef std::function<void ()> WriteCallbackType;

    HTTPConnection(const HTTPConnection &) = delete;

    HTTPConnection &operator=(const HTTPConnection &) = delete;

    HTTPConnection(std::shared_ptr<BaseIOStream> stream, std::string address,
                   const RequestCallbackType &requestCallback,
                   bool noKeepAlive = false,
                   bool xheaders = false);

    ~HTTPConnection();

    void start();

    void write(const Byte *chunk, size_t length, WriteCallbackType callback= nullptr);

    void finish();

    bool getXHeaders() const {
        return _xheaders;
    }

    std::weak_ptr<BaseIOStream> getStream() const {
        return _stream;
    }

    std::shared_ptr<BaseIOStream> fetchStream() const {
        return _stream.lock();
    }

    std::shared_ptr<HTTPServerRequest> fetchRequest() const {
        return _requestObserver.lock();
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPConnection> create(Args&& ...args) {
        return std::make_shared<HTTPConnection>(std::forward<Args>(args)...);
    }
protected:
    void onWriteComplete();

    void finishRequest();

    void onHeaders(ByteArray data);

    void onRequestBody(ByteArray data);

    std::weak_ptr<BaseIOStream> _stream;
    std::string _address;
    const RequestCallbackType &_requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    std::weak_ptr<HTTPServerRequest> _requestObserver;
    std::shared_ptr<HTTPServerRequest> _request;
    bool _requestFinished{false};
    WriteCallbackType _writeCallback;
};


class HTTPServerRequest {
public:
    typedef HTTPUtil::RequestFilesType RequestFilesType;
    typedef HTTPUtil::QueryArgumentsType QueryArgumentsType;
    typedef HTTPConnection::WriteCallbackType WriteCallbackType;
    typedef boost::optional<SimpleCookie> CookiesType;

    HTTPServerRequest(const HTTPServerRequest &) = delete;

    HTTPServerRequest &operator=(const HTTPServerRequest &) = delete;

    HTTPServerRequest(std::shared_ptr<HTTPConnection> connection,
                      std::string method,
                      std::string uri,
                      std::string version = "HTTP/1.0",
                      std::unique_ptr<HTTPHeaders> &&headers = nullptr,
                      ByteArray body = {},
                      std::string remoteIp = {},
                      std::string protocol = {},
                      std::string host = {},
                      RequestFilesType files = {});

    ~HTTPServerRequest();

    bool supportsHTTP1_1() const {
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

    float requestTime() const;

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

    void setBody(ByteArray body) {
        _body = std::move(body);
    }

    const ByteArray& getBody() const {
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

    const RequestFilesType& getFiles() const {
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

    QueryArgumentsType& arguments() {
        return _arguments;
    }

    const QueryArgumentsType& getArguments() const {
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

    RequestFilesType& files() {
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
    ByteArray _body;
    std::string _remoteIp;
    std::string _protocol;
    std::string _host;
    RequestFilesType _files;
    std::shared_ptr<HTTPConnection> _connection;
    Timestamp _startTime;
    Timestamp _finishTime;
    std::string _path;
    std::string _query;
    QueryArgumentsType _arguments;
    mutable CookiesType _cookies;
};


#endif //TINYCORE_HTTPSERVER_H
