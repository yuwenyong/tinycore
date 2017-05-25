//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/asyncio/httpserver.h"
#include <boost/utility/string_ref.hpp>
#include "tinycore/asyncio/ioloop.h"
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
        : _requestCallback(std::move(requestCallback))
        , _noKeepAlive(noKeepAlive)
        , _ioloop(ioloop ? ioloop : sIOLoop)
        , _xheaders(xheaders)
        , _sslOption(std::move(sslOption))
        , _acceptor(_ioloop->getService())
        , _socket(_ioloop->getService()){

}

HTTPServer::~HTTPServer() {

}

void HTTPServer::listen(unsigned short port, std::string address) {
    bind(port, std::move(address));
    start();
}

void HTTPServer::bind(unsigned short port, std::string address) {
    BaseIOStream::ResolverType resolver(_ioloop->getService());
    BaseIOStream::ResolverType::query query(address, std::to_string(port));
    BaseIOStream::EndPointType endpoint = *resolver.resolve(query);
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen();
}

void HTTPServer::stop() {
    _acceptor.close();
}

void HTTPServer::asyncAccept() {
    _acceptor.async_accept(_socket, [this](const boost::system::error_code &ec) {
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                throw boost::system::system_error(ec);
            } else {
                return;
            }
        } else {
            try {
                std::shared_ptr<BaseIOStream> stream;
                if (_sslOption) {
                    stream = SSLIOStream::create(std::move(_socket), _sslOption, _ioloop);
                } else {
                    stream = IOStream::create(std::move(_socket), _ioloop);
                }
                auto connection = HTTPConnection::create(stream, stream->getRemoteAddress(), _requestCallback,
                                                         _noKeepAlive, _xheaders);
                connection->start();
            } catch (std::exception &e) {
                Log::error("Error in connection callback:%s", e.what());
            }
            asyncAccept();
        }
    });
}


class _BadRequestException: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


HTTPConnection::HTTPConnection(std::shared_ptr<BaseIOStream> stream,
                               std::string address,
                               const RequestCallbackType &requestCallback,
                               bool noKeepAlive,
                               bool xheaders)
        : _streamObserver(stream)
        , _stream(std::move(stream))
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
    ASSERT(_stream);
    auto stream = std::move(_stream);
    stream->readUntil("\r\n\r\n", std::bind(&HTTPConnection::onHeaders, shared_from_this(), std::placeholders::_1,
                                            std::placeholders::_2));
}

void HTTPConnection::write(const Byte *chunk, size_t length) {
    ASSERT(fetchRequest(), "Request closed");
    auto stream = fetchStream();
    if (stream && !stream->closed()) {
        _stream.reset();
        stream->write(chunk, length, std::bind(&HTTPConnection::onWriteComplete, shared_from_this()));
    }
}

void HTTPConnection::finish() {
    ASSERT(fetchRequest(), "Request closed");
    _requestFinished = true;
    auto stream = fetchStream();
    _request = fetchRequest();
    if (stream && !stream->writing()) {
        finishRequest();
    }
}

void HTTPConnection::onWriteComplete() {
    ASSERT(!_stream);
    auto stream = fetchStream();
    ASSERT(stream);
    if (stream->dying()) {
        _stream = std::move(stream);
    }
    if (_requestFinished) {
        finishRequest();
    }
}

void HTTPConnection::finishRequest() {
    ASSERT(_request);
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
        auto stream = fetchStream();
        ASSERT(stream);
        stream->close();
        return;
    }
    start();
}

void HTTPConnection::onHeaders(Byte *data, size_t length) {
    ASSERT(!_stream);
    auto stream = fetchStream();
    ASSERT(stream);
    if (stream->dying()) {
        _stream = stream;
    }
    try {
        const char *eol = StrNStr((char *)data, length, "\r\n");
        std::string startLine, rest;
        if (eol) {
            startLine.assign((const char *)data, eol);
            rest.assign(eol, (const char *)data + length);
        } else {
            startLine.assign((char *)data, length);
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
            if (contentLength > stream->getMaxBufferSize()) {
                throw _BadRequestException("Content-Length too long");
            }
            if (requestHeaders->get("Expect") == "100-continue") {
                const char *continueLine = "HTTP/1.1 100 (Continue)\r\n\r\n";
                stream->write((const Byte *)continueLine, strlen(continueLine));
            }
            _stream.reset();
            stream->readBytes(contentLength, std::bind(&HTTPConnection::onRequestBody, shared_from_this(),
                                                       std::placeholders::_1, std::placeholders::_2));
            return;
        }
        _request->setConnection(shared_from_this());
        _requestCallback(std::move(_request));
    } catch (_BadRequestException &e) {
        Log::info("Malformed HTTP request from %s: %s", _address.c_str(), e.what());
        stream->close();
    }
}

void HTTPConnection::onRequestBody(Byte *data, size_t length) {
    ASSERT(!_stream);
    auto stream = fetchStream();
    ASSERT(stream);
    if (stream->dying()) {
        _stream = stream;
    }
    ASSERT(_request);
    _request->setBody(ByteArray(data, data + length));
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
            StringVector fields = String::split(contentType, ';');
            bool found = false;
            std::string k, sep, v;
            for (auto &field: fields) {
                boost::trim(field);
                std::tie(k, sep, v) = String::partition(field, "=");
                if (k == "boundary" && !v.empty()) {
                    parseMimeBody(v, data, length);
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

void HTTPConnection::parseMimeBody(const std::string &boundary, const Byte *data, size_t length) {
    ASSERT(_request);
    std::string realBoundary;
    if (boost::starts_with(boundary, "\"") && boost::ends_with(boundary, "\"")) {
        if (boundary.length() >= 2) {
            realBoundary = boundary.substr(1, boundary.length() - 2);
        }
    } else {
        realBoundary = boundary;
    }
    size_t footerLength;
    if (length >= 2 && (char)data[length - 2] == '\r' && (char)data[length - 1] == '\n') {
        footerLength = realBoundary.length() + 6;
    } else {
        footerLength = realBoundary.length() + 4;
    }
    if (length <= footerLength) {
        return;
    }
    std::string sep("--" + realBoundary + "\r\n");
    const Byte *beg = data, *end = data + length - footerLength, *cur, *val;
    size_t eoh, eon, valSize;
    std::string part, nameHeader, name, nameValue, ctype;
    HTTPHeaders headers;
    StringMap nameValues;
    StringVector nameParts;
    decltype(nameValues.begin()) nameIter, fileNameIter;
    for (; true; beg = cur + sep.size()) {
        cur = std::search(beg, end, (const Byte *)sep.data(), (const Byte *)sep.data() + sep.size());
        if (cur == end) {
            break;
        }
        if (cur == beg) {
            continue;
        }
        part.assign(beg, cur);
        eoh = part.find("\r\n\r\n");
        if (eoh == std::string::npos) {
            Log::warn("multipart/form-data missing headers");
            continue;
        }
        headers.clear();
        headers.parseLines(part.substr(0, eoh));
        nameHeader = headers.get("Content-Disposition");
        if (!boost::starts_with(nameHeader, "form-data;") || !boost::ends_with(part, "\r\n")) {
            Log::warn("Invalid multipart/form-data");
            continue;
        }
        if (part.length() <= eoh + 6) {
            val = (const Byte *)part.data();
            valSize = 0;
        } else {
            val = (const Byte *)part.data() + eoh + 4;
            valSize = part.size() - eoh - 6;
        }
        nameValues.clear();
        nameHeader = nameHeader.substr(10);
        nameParts = String::split(nameHeader, ';');
        for (auto &namePart: nameParts) {
            boost::trim(namePart);
            eon = namePart.find('=');
            if (eon == std::string::npos) {
                ThrowException(ValueError, "need more than 2 values to unpack");
            }
            name = namePart.substr(0, eon);
            nameValue = namePart.substr(eon + 1);
            boost::trim(nameValue);
            nameValues.emplace(std::move(name), std::move(nameValue));
        }
        nameIter = nameValues.find("name");
        if (nameIter == nameValues.end()) {
            Log::warn("multipart/form-data value missing name");
            continue;
        }
        name = nameIter->second;
        fileNameIter = nameValues.find("filename");
        if (fileNameIter != nameValues.end()) {
            ctype = headers.get("Content-Type", "application/unknown");
            _request->addFile(name, HTTPFile(std::move(fileNameIter->second), std::move(ctype),
                                             ByteArray(val, val + valSize)));
        } else {
            _request->addArgument(name, std::string((const char *)val, valSize));
        }
    }
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
                                     RequestFilesType files)
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
        } else if (connection && std::dynamic_pointer_cast<SSLIOStream>(connection->fetchStream())) {
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
    std::string scheme, netloc, fragment;
    std::tie(scheme, netloc, _path, _query, fragment) = URLParse::urlSplit(_uri);
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

void HTTPServerRequest::write(const Byte *chunk, size_t length) {
    ASSERT(_connection);
    _connection->write(chunk, length);
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

void HTTPServerRequest::addArgument(const std::string &name, std::string value) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        iter->second.emplace_back(std::move(value));
    } else {
        _arguments[name].emplace_back(std::move(value));
    }
}

void HTTPServerRequest::addArguments(const std::string &name, StringVector values) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        for (auto &value: values) {
            iter->second.emplace_back(std::move(value));
        }
    } else {
        _arguments.emplace(std::move(name), std::move(values));
    }
}

void HTTPServerRequest::addFile(const std::string &name, HTTPFile file) {
    auto iter = _files.find(name);
    if (iter != _files.end()) {
        iter->second.emplace_back(std::move(file));
    } else {
        _files[name].emplace_back(std::move(file));
    }
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
