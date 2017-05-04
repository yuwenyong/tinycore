//
// Created by yuwenyong on 17-3-28.
//

#ifndef TINYCORE_HTTPSERVER_H
#define TINYCORE_HTTPSERVER_H

#include "tinycore/common/common.h"
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "tinycore/networking/iostream.h"
#include "tinycore/networking/httputil.h"
#include "tinycore/utilities/urlparse.h"


class HTTPRequest;
typedef std::shared_ptr<HTTPRequest> HTTPRequestPtr;
typedef std::weak_ptr<HTTPRequest> HTTPRequestWPtr;


class SSLOption {
public:
    typedef boost::asio::ssl::context SSLContextType;

    SSLOption()
            :_context(boost::asio::ssl::context::sslv23) {
        _context.set_options(boost::asio::ssl::context::no_sslv3);
    }

    SSLOption(const std::string &certFile, const std::string &keyFile)
            :SSLOption() {
        _context.use_certificate_chain_file(certFile);
        _context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    }

    ~SSLOption() {

    }

    void setCertFile(const std::string &certFile) {
        _context.use_certificate_chain_file(certFile);
    }

    void setKeyFile(const std::string &keyFile) {
        _context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    }

    SSLContextType & getContext() {
        return _context;
    }
protected:
    SSLContextType _context;
};


class HTTPServer {
public:
    typedef boost::asio::ip::tcp::acceptor AcceptorType;
    typedef std::function<void (HTTPRequestPtr)> RequestCallbackType;

    HTTPServer(const HTTPServer &) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;
    HTTPServer(RequestCallbackType requestCallback,
               bool noKeepAlive=false,
               IOLoop *ioloop=nullptr,
               bool xheaders=false,
               SSLOption *sslOption=nullptr);
    ~HTTPServer();
    void listen(unsigned short port, std::string address="0.0.0.0");
    void bind(unsigned short port, std::string address);

    void start() {
        doAccept();
    }

    void stop();
protected:
    void doAccept();

    RequestCallbackType _requestCallback;
    bool _noKeepAlive;
    IOLoop *_ioloop;
    bool _xheaders;
    SSLOption * _sslOption;
    AcceptorType _acceptor;
    BaseIOStream::SocketType _socket;
};

typedef std::unique_ptr<HTTPServer> HTTPServerPtr;


class HTTPConnection: public std::enable_shared_from_this<HTTPConnection> {
public:
    typedef BaseIOStream::BufferType BufferType;
    typedef HTTPServer::RequestCallbackType RequestCallbackType;

    HTTPConnection(const HTTPConnection &) = delete;
    HTTPConnection &operator=(const HTTPConnection &) = delete;
    HTTPConnection(BaseIOStreamPtr stream, std::string address,
                   const RequestCallbackType &requestCallback,
                   bool noKeepAlive=false,
                   bool xheaders=false);
    ~HTTPConnection();

    void start();
    void write(BufferType &chunk);
    void finish();

    bool getXHeaders() const {
        return _xheaders;
    }

    BaseIOStreamPtr getStream() {
        return _stream.lock();
    }

    HTTPRequestPtr getRequest() {
        return _request.lock();
    }

protected:
    void onWriteComplete();
    void finishRequest();
    void onHeaders(BufferType &data);
    void onRequestBody(BufferType &data);
    void parseMimeBody(std::string boundary, std::string data);

    BaseIOStreamWPtr _stream;
    BaseIOStreamPtr _streamKeeper;
    std::string _address;
    const RequestCallbackType &_requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    HTTPRequestWPtr _request;
    HTTPRequestPtr _requestKeeper;
    bool _requestFinished{false};
};


typedef std::shared_ptr<HTTPConnection> HTTPConnectionPtr;
typedef std::weak_ptr<HTTPConnection> HTTPConnectionWPtr;


class HTTPRequest {
public:
    typedef std::map<std::string, std::vector<HTTPFile>> RequestFilesType ;
    typedef std::chrono::steady_clock ClockType;
    typedef ClockType::time_point TimePointType;
    typedef HTTPConnection::BufferType BufferType;
    typedef URLParse::QueryArguments QueryArgumentsType;

    HTTPRequest(const HTTPRequest&) = delete;
    HTTPRequest& operator=(const HTTPRequest&) = delete;

    HTTPRequest(HTTPConnectionPtr connection,
                std::string method,
                std::string uri,
                std::string version = "HTTP/1.0",
                HTTPHeadersPtr &&headers = nullptr,
                std::string body = {},
                std::string remoteIp = {},
                std::string protocol = {},
                std::string host = {},
                RequestFilesType files = {});
    ~HTTPRequest();

    bool supportsHTTP1_1() const {
        return _version == "HTTP/1.1";
    }

    void write(const char *chunk, size_t length);

    void write(const char *chunk) {
        write(chunk, strlen(chunk));
    }

    void write(const std::string &chunk) {
        write(chunk.c_str(), chunk.length());
    }

    void write(const std::vector<char> &chunk) {
        write(chunk.data(), chunk.size());
    }

    void finish();

    std::string fullURL() const {
        return _protocol + "://" + _host + _uri;
    }

    float requestTime() const;

    const HTTPHeaders* getHTTPHeader() const {
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

    void setBody(const char* body, size_t length) {
        _body.assign(body, length);
    }

    const std::string& getBody() const {
        return _body;
    }

    const std::string& getRemoteIp() const {
        return _remoteIp;
    }

    const std::string& getHost() const {
        return _host;
    }

    HTTPConnectionPtr getConnection() {
        return _connection;
    }

    void setConnection(HTTPConnectionPtr connection) {
        _connection = std::move(connection);
    }

    const std::string& getPath() const {
        return _path;
    }

    const QueryArgumentsType& getArguments() const {
        return _arguments;
    }

    void addArgument(std::string name, std::string value);
    void addArguments(std::string name, StringVector values);
    void addFile(std::string name, HTTPFile file);
    std::string dump() const;
protected:
    std::string _method;
    std::string _uri;
    std::string _version;
    HTTPHeadersPtr _headers;
    std::string _body;
    std::string _remoteIp;
    std::string _protocol;
    std::string _host;
    RequestFilesType  _files;
    HTTPConnectionPtr _connection;
    TimePointType _startTime;
    TimePointType _finishTime;
    std::string _path;
    std::string _query;
    QueryArgumentsType _arguments;
};


#endif //TINYCORE_HTTPSERVER_H
