//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/asyncio/httpserver.h"
#include <boost/utility/string_ref.hpp>
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/httputils/urlparse.h"
#include "tinycore/utilities/string.h"
#include "tinycore/common/errors.h"
#include "tinycore/logging/log.h"


HTTPServer::HTTPServer(RequestCallbackType requestCallback,
                       bool noKeepAlive,
                       IOLoop *ioloop,
                       bool xheaders,
                       std::shared_ptr<SSLOption> sslOption)
        : TCPServer(ioloop, std::move(sslOption))
        , _requestCallback(std::move(requestCallback))
        , _noKeepAlive(noKeepAlive)
        , _xheaders(xheaders) {

}

HTTPServer::~HTTPServer() {

}

void HTTPServer::handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) {
    auto connection = HTTPConnection::create(std::move(stream), std::move(address), _requestCallback, _noKeepAlive,
                                             _xheaders);
    connection->start();
}


class _BadRequestException: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


HTTPConnection::HTTPConnection(std::shared_ptr<BaseIOStream> stream,
                               std::string address,
                               RequestCallbackType &requestCallback,
                               bool noKeepAlive,
                               bool xheaders)
        : _stream(std::move(stream))
        , _address(std::move(address))
        , _requestCallback(requestCallback)
        , _noKeepAlive(noKeepAlive)
        , _xheaders(xheaders) {
#ifndef NDEBUG
    sWatcher->inc(SYS_HTTPCONNECTION_COUNT);
#endif
}

HTTPConnection::~HTTPConnection() {
#ifndef NDEBUG
    sWatcher->dec(SYS_HTTPCONNECTION_COUNT);
#endif
}

void HTTPConnection::start() {
    BaseIOStream::ReadCallbackType callback = std::bind(&HTTPConnection::onHeaders, this, std::placeholders::_1);
    _headerCallback = StackContext::wrap<ByteArray>(std::move(callback));
    NullContext ctx;
    _stream->readUntil("\r\n\r\n", [this, self=shared_from_this()](ByteArray data) {
        _headerCallback(std::move(data));
    });
}

void HTTPConnection::write(const Byte *chunk, size_t length, WriteCallbackType callback) {
    ASSERT(!_requestObserver.expired(), "Request closed");
    if (!_stream->closed()) {
        _writeCallback = StackContext::wrap(std::move(callback));
        if (_writeCallback) {
            auto wrapper = std::make_shared<Wrapper<>>(shared_from_this(), [this]() {
               onWriteComplete();
            });
            _stream->write(chunk, length, std::bind(&Wrapper<>::operator(), std::move(wrapper)));
        } else {
            _stream->write(chunk, length, std::bind(&HTTPConnection::onWriteComplete, shared_from_this()));
        }
    }
}

void HTTPConnection::finish() {
    ASSERT(!_requestObserver.expired(), "Request closed");
    _request = _requestObserver.lock();
    _requestFinished = true;
    if (_stream->writing()) {
        finishRequest();
    }
}

void HTTPConnection::onWriteComplete() {
    if (_writeCallback) {
        WriteCallbackType callback(std::move(_writeCallback));
        _writeCallback = nullptr;
        callback();
    }
    if (_requestFinished && !_stream->writing()) {
        finishRequest();
    }
}

void HTTPConnection::finishRequest() {
    bool disconnect;
    if (_noKeepAlive) {
        disconnect = true;
    } else {
        auto headers = _request->getHTTPHeaders();
        std::string connectionHeader = headers->get("Connection");
        if (_request->supportsHTTP1_1()) {
            disconnect = connectionHeader == "close";
        } else if (headers->has("Content-Length")
                   || _request->getMethod() == "HEAD"
                   || _request->getMethod() == "GET") {
            disconnect = connectionHeader != "Keep-Alive";
        } else {
            disconnect = true;
        }
    }
    _request.reset();
    _requestFinished = false;
    if (disconnect) {
        close();
        return;
    }
    try {
        NullContext ctx;
        _stream->readUntil("\r\n\r\n", [this, self=shared_from_this()](ByteArray data) {
            _headerCallback(std::move(data));
        });
    } catch (IOError &e) {
        close();
    }
}

void HTTPConnection::onHeaders(ByteArray data) {
    try {
        const char *eol = StrNStr((char *)data.data(), data.size(), "\r\n");
        std::string startLine, rest;
        if (eol) {
            startLine.assign((const char *)data.data(), eol);
            rest.assign(eol, (const char *)data.data() + data.size());
        } else {
            startLine.assign((char *)data.data(), data.size());
        }
        StringVector requestLineComponents = String::split(startLine);
        if (requestLineComponents.size() != 3) {
            throw _BadRequestException("Malformed HTTP request line");
        }
        std::string method = std::move(requestLineComponents[0]);
        std::string uri = std::move(requestLineComponents[1]);
        std::string version = std::move(requestLineComponents[2]);
        if (!boost::starts_with(version, "HTTP/")) {
            throw _BadRequestException("Malformed HTTP version in HTTP Request-Line");
        }
        auto headers = HTTPHeaders::parse(rest);
        _request = HTTPServerRequest::create(shared_from_this(), std::move(method), std::move(uri), std::move(version),
                                             std::move(headers), ByteArray(), _address);
        _requestObserver = _request;
        auto requestHeaders = _request->getHTTPHeaders();
        std::string contentLengthValue = requestHeaders->get("Content-Length");
        if (!contentLengthValue.empty()) {
            size_t contentLength = (size_t) std::stoi(contentLengthValue);
            if (contentLength > _stream->getMaxBufferSize()) {
                throw _BadRequestException("Content-Length too long");
            }
            if (requestHeaders->get("Expect") == "100-continue") {
                const char *continueLine = "HTTP/1.1 100 (Continue)\r\n\r\n";
                _stream->write((const Byte *)continueLine, strlen(continueLine));
            }
            _stream->readBytes(contentLength, [this, self=shared_from_this()](ByteArray data) {
                onRequestBody(std::move(data));
            });
            return;
        }
        _request->setConnection(shared_from_this());
        _requestCallback(std::move(_request));
    } catch (_BadRequestException &e) {
        Log::info("Malformed HTTP request from %s: %s", _address.c_str(), e.what());
        _stream->close();
    }
}

void HTTPConnection::onRequestBody(ByteArray data) {
    _request->setBody(std::move(data));
    auto headers = _request->getHTTPHeaders();
    std::string contentType = headers->get("Content-Type");
    const std::string &method = _request->getMethod();
    if (method == "POST" || method == "PUT") {
        if (boost::starts_with(contentType, "application/x-www-form-urlencoded")) {
            const ByteArray &body = _request->getBody();
            auto arguments = URLParse::parseQS(std::string((const char *)body.data(), body.size()), false);
            for (auto &nv: arguments) {
                if (!nv.second.empty()) {
                    _request->addArguments(nv.first, std::move(nv.second));
                }
            }
        } else if (boost::starts_with(contentType, "multipart/form-data")) {
            const ByteArray &body = _request->getBody();
            StringVector fields = String::split(contentType, ';');
            bool found = false;
            std::string k, sep, v;
            for (auto &field: fields) {
                boost::trim(field);
                std::tie(k, sep, v) = String::partition(field, "=");
                if (k == "boundary" && !v.empty()) {
                    HTTPUtil::parseMultipartFormData(std::move(v), body, _request->arguments(), _request->files());
                    found = true;
                    break;
                }
            }
            if (!found) {
                Log::warn("Invalid multipart/form-data");
            }
        }
    }
    _request->setConnection(shared_from_this());
    _requestCallback(std::move(_request));
}


HTTPServerRequest::HTTPServerRequest(std::shared_ptr<HTTPConnection> connection,
                                     std::string method,
                                     std::string uri,
                                     std::string version,
                                     std::unique_ptr<HTTPHeaders> &&headers,
                                     ByteArray body,
                                     std::string remoteIp,
                                     std::string protocol,
                                     std::string host,
                                     HTTPFileListMap files)
        : _method(std::move(method))
        , _uri(std::move(uri))
        , _version(std::move(version))
        , _headers(std::move(headers))
        , _body(std::move(body))
        , _files(std::move(files))
//        , _connection(std::move(connection))
        , _startTime(TimestampClock::now())
        , _finishTime(Timestamp::min()) {
    if (connection && connection->getXHeaders()) {
        _remoteIp = _headers->get("X-Forwarded-For", remoteIp);
        _remoteIp = _headers->get("X-Real-Ip", _remoteIp);
        _protocol = _headers->get("X-Forwarded-Proto", protocol);
        _protocol = _headers->get("X-Scheme", _protocol);
        if (_protocol != "http" && _protocol != "https") {
            _protocol = "http";
        }
    } else {
        _remoteIp = std::move(remoteIp);
        if (!protocol.empty()) {
            _protocol = std::move(protocol);
        } else if (connection && std::dynamic_pointer_cast<SSLIOStream>(connection->getStream())) {
            _protocol = "https";
        } else {
            _protocol = "http";
        }
    }
    if (!host.empty()) {
        _host = std::move(host);
    } else {
        _host = _headers->get("Host", "127.0.0.1");
    }
    std::tie(std::ignore, std::ignore, _path, _query, std::ignore) = URLParse::urlSplit(_uri);;
    _arguments = URLParse::parseQS(_query, false);
#ifndef NDEBUG
    sWatcher->inc(SYS_HTTPSERVERREQUEST_COUNT);
#endif
}

HTTPServerRequest::~HTTPServerRequest() {
#ifndef NDEBUG
    sWatcher->dec(SYS_HTTPSERVERREQUEST_COUNT);
#endif
}

const SimpleCookie& HTTPServerRequest::cookies() const {
    if (!_cookies) {
        _cookies.emplace();
        if (_headers->has("Cookie")) {
            try {
                _cookies->load(_headers->at("Cookie"));
            } catch (...) {
                _cookies->clear();
            }
        }
    }
    return _cookies.get();
}

void HTTPServerRequest::write(const Byte *chunk, size_t length, WriteCallbackType callback) {
    ASSERT(_connection);
    _connection->write(chunk, length, std::move(callback));
}

void HTTPServerRequest::finish() {
    ASSERT(_connection);
    auto connection = std::move(_connection);
    connection->finish();
    _finishTime = TimestampClock::now();
}

float HTTPServerRequest::requestTime() const {
    std::chrono::milliseconds elapse;
    if (_finishTime == Timestamp::min()) {
        elapse = std::chrono::duration_cast<std::chrono::milliseconds>(TimestampClock::now() - _startTime);
    } else {
        elapse = std::chrono::duration_cast<std::chrono::milliseconds>(_finishTime - _startTime);
    }
    return elapse.count() / 1000 + elapse.count() % 1000 / 1000.0f;
}

std::string HTTPServerRequest::dump() const {
    StringVector argsList;
    argsList.emplace_back("protocol=" + _protocol);
    argsList.emplace_back("host=" + _host);
    argsList.emplace_back("method=" + _method);
    argsList.emplace_back("uri=" + _uri);
    argsList.emplace_back("version=" + _version);
    argsList.emplace_back("remoteIp=" + _remoteIp);
    argsList.emplace_back("body=" + std::string((const char *)_body.data(), _body.size()));
    std::string args = boost::join(argsList, ",");
    StringVector headersList;
    _headers->getAll([&headersList](const std::string &name, const std::string &value){
         headersList.emplace_back("\"" + name + "\": \"" + value + "\"");
    });
    std::string headers = boost::join(headersList, ", ");
    return String::format("HTTPRequest(%s, headers=\"{%s}\"", args.c_str(), headers.c_str());
}
