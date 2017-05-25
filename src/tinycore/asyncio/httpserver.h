//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_HTTPSERVER_H
#define TINYCORE_HTTPSERVER_H

#include "tinycore/common/common.h"
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/httputil.h"
#include "tinycore/httputils/urlparse.h"


class HTTPServerRequest;

class HTTPServer {
public:
    typedef boost::asio::ip::tcp::acceptor AcceptorType;
    typedef std::function<void(std::shared_ptr<HTTPServerRequest>)> RequestCallbackType;

    HTTPServer(const HTTPServer &) = delete;

    HTTPServer &operator=(const HTTPServer &) = delete;

    HTTPServer(RequestCallbackType requestCallback,
               bool noKeepAlive = false,
               IOLoop *ioloop = nullptr,
               bool xheaders = false,
               std::shared_ptr<SSLOption> sslOption = nullptr);

    ~HTTPServer();

    void listen(unsigned short port, std::string address = "0.0.0.0");

    void bind(unsigned short port, std::string address);

    void start() {
        asyncAccept();
    }

    void stop();
protected:
    void asyncAccept();

    RequestCallbackType _requestCallback;
    bool _noKeepAlive;
    IOLoop *_ioloop;
    bool _xheaders;
    std::shared_ptr<SSLOption> _sslOption;
    AcceptorType _acceptor;
    BaseIOStream::SocketType _socket;
};


class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
public:
    typedef HTTPServer::RequestCallbackType RequestCallbackType;

    HTTPConnection(const HTTPConnection &) = delete;

    HTTPConnection &operator=(const HTTPConnection &) = delete;

    HTTPConnection(std::shared_ptr<BaseIOStream> stream, std::string address,
                   const RequestCallbackType &requestCallback,
                   bool noKeepAlive = false,
                   bool xheaders = false);

    ~HTTPConnection();

    void start();

    void write(const Byte *chunk, size_t length);

    void finish();

    bool getXHeaders() const {
        return _xheaders;
    }

    std::shared_ptr<BaseIOStream> fetchStream() {
        return _streamObserver.lock();
    }

    std::shared_ptr<BaseIOStream> releaseStream() {
        ASSERT(_stream);
        return std::move(_stream);
    }

    std::shared_ptr<HTTPServerRequest> fetchRequest() {
        return _requestObserver.lock();
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPConnection> create(Args&& ...args) {
        return std::make_shared<HTTPConnection>(std::forward<Args>(args)...);
    }
protected:
    void onWriteComplete();

    void finishRequest();

    void onHeaders(Byte *data, size_t length);

    void onRequestBody(Byte *data, size_t length);

    void parseMimeBody(const std::string &boundary, const Byte *data, size_t length);

    std::weak_ptr<BaseIOStream> _streamObserver;
    std::shared_ptr<BaseIOStream> _stream;
    std::string _address;
    const RequestCallbackType &_requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    std::weak_ptr<HTTPServerRequest> _requestObserver;
    std::shared_ptr<HTTPServerRequest> _request;
    bool _requestFinished{false};
};


class HTTPServerRequest {
public:
    typedef std::map<std::string, std::vector<HTTPFile>> RequestFilesType;
    typedef URLParse::QueryArguments QueryArgumentsType;

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

    void write(const Byte *chunk, size_t length);

    void write(const ByteArray &chunk) {
        write(chunk.data(), chunk.size());
    }

    void write(const char *chunk) {
        write((const Byte *)chunk, strlen(chunk));
    }

    void write(const std::string &chunk) {
        write((const Byte *)chunk.data(), chunk.length());
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

    const QueryArgumentsType& getArguments() const {
        return _arguments;
    }

    void addArgument(const std::string &name, std::string value);

    void addArguments(const std::string &name, StringVector values);

    void addFile(const std::string &name, HTTPFile file);

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
};


#endif //TINYCORE_HTTPSERVER_H
