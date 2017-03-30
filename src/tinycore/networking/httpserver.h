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


class HTTPRequest;
typedef std::unique_ptr<HTTPRequest> HTTPRequestUPtr;


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

    HTTPServer(const HTTPServer &) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;
    HTTPServer(std::function<void (HTTPRequest*)> requestCallback,
               bool noKeepAlive=false,
               IOLoop *ioloop=nullptr,
               bool xheaders=false,
               SSLOption *sslOption=nullptr);
    ~HTTPServer();
    int listen(unsigned short port, const std::string &address="0.0.0.0");
    int stop();
protected:
    int _bind(unsigned short port, const std::string &address);

    int _start() {
        _doAccept();
        return 0;
    }

    void _doAccept();

    std::function<void (HTTPRequest*)> _requestCallback;
    bool _noKeepAlive;
    IOLoop *_ioloop;
    bool _xheaders;
    SSLOption * _sslOption;
    AcceptorType _acceptor;
    BaseIOStream::SocketType _socket;
};


class _BadRequestException: public Exception {
public:
    using Exception::Exception;
};


class HTTPConnection: public std::enable_shared_from_this<HTTPConnection> {
public:
    typedef BaseIOStream::ConstBufferType ConstBufferType;

    HTTPConnection(const HTTPConnection &) = delete;
    HTTPConnection &operator=(const HTTPConnection &) = delete;
    HTTPConnection(BaseIOStreamPtr stream, std::string address,
                   std::function<void (HTTPRequest *)> requestCallback,
                   bool noKeepAlive=false,
                   bool xheaders=false);

    bool getXHeaders() const {
        return _xheaders;
    }

    BaseIOStreamPtr getStream() {
        return _stream;
    }
    int start();
    int write(const char *chunk, size_t length);
    int finish();

protected:
    void _onWriteComplete();
    void _finishRequest();
    void _onHeaders(const char *data, size_t length);
    void _onRequestBody(const char *data, size_t length);
    void _parseMimeBody(std::string boundary, std::string data);

    BaseIOStreamPtr _stream;
    std::string _address;
    std::function<void (HTTPRequest *)> _requestCallback;
    bool _noKeepAlive;
    bool _xheaders;
    HTTPRequestUPtr _request;
    bool _requestFinished{false};
};

typedef std::shared_ptr<HTTPConnection> HTTPConnectionPtr;


class HTTPFile {
public:
    HTTPFile(std::string fileName,
             std::string contentType,
             std::string body)
            : _fileName(std::move(fileName))
            , _contentType(std::move(contentType))
            , _body(std::move(body)) {

    }

protected:
    std::string _fileName;
    std::string _contentType;
    std::string _body;
};

class HTTPRequest {
public:
    typedef std::map<std::string, StringVector> RequestArguments;
    typedef std::vector<std::pair<std::string, std::string>> RequestArgumentsList;
    typedef std::map<std::string, std::vector<HTTPFile>> RequestFiles ;
    typedef std::chrono::steady_clock::time_point TimePoint;

    HTTPRequest(const HTTPRequest&) = delete;
    HTTPRequest& operator=(const HTTPRequest&) = delete;

    HTTPRequest(HTTPConnectionPtr connection,
                std::string method,
                std::string uri,
                std::string version = "HTTP/1.0",
                HTTPHeaderUPtr &&headers = nullptr,
                std::string body = {},
                std::string remoteIp = {},
                std::string protocol = {},
                std::string host = {},
                RequestFiles files = {});

    ~HTTPRequest();

    bool supportsHTTP11() const {
        return _version == "HTTP/1.1";
    }

    int write(const char *chunk, size_t length);

    int write(const std::string &chunk) {
        return write(chunk.c_str(), chunk.length());
    }

    int write(const std::vector<char> &chunk) {
        return write(chunk.data(), chunk.size());
    }

    int finish();

    std::string fullURL() const {
        return _protocol + "://" + _host + _uri;
    }

    float requestTime() const;

    const HTTPHeader* getHTTPHeader() const {
        return _headers.get();
    }

    const std::string& getMethod() const {
        return _method;
    }

    void setBody(const char* body, size_t length) {
        _body.assign(body, length);
    }

    void addArgument(std::string name, std::string value);
    void addArguments(std::string name, StringVector values);
    void addFile(std::string name, HTTPFile file);
protected:
    std::string _method;
    std::string _uri;
    std::string _version;
    HTTPHeaderUPtr _headers;
    std::string _body;
    std::string _remoteIp;
    std::string _protocol;
    std::string _host;
    RequestFiles  _files;
    HTTPConnectionPtr _connection;
    TimePoint _startTime;
    TimePoint _finishTime;
    std::string _path;
    std::string _query;
    RequestArguments _arguments;
};


#endif //TINYCORE_HTTPSERVER_H
