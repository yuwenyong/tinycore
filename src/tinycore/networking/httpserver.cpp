//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/networking/httpserver.h"
#include <boost/utility/string_ref.hpp>
#include "tinycore/networking/ioloop.h"
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
                       SSLOptionPtr sslOption)
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

void HTTPServer::doAccept() {
    _acceptor.async_accept(_socket, [this](const boost::system::error_code &ec) {
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                throw boost::system::system_error(ec);
            } else {
                return;
            }
        } else {
            try {
                BaseIOStreamPtr stream;
                if (_sslOption) {
                    stream = std::make_shared<SSLIOStream>(std::move(_socket), _sslOption, _ioloop);
                } else {
                    stream = std::make_shared<IOStream>(std::move(_socket), _ioloop);
                }
                auto connection = std::make_shared<HTTPConnection>(stream,
                                                                   stream->getRemoteAddress(),
                                                                   _requestCallback,
                                                                   _noKeepAlive,
                                                                   _xheaders);
                connection->start();
            } catch (std::exception &e) {
                Log::error("Error in connection callback:%s", e.what());
            }
            doAccept();
        }
    });
}


class _BadRequestException: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


HTTPConnection::HTTPConnection(BaseIOStreamPtr stream,
                               std::string address,
                               const RequestCallbackType &requestCallback,
                               bool noKeepAlive,
                               bool xheaders)
        : _stream(stream)
        , _streamKeeper(std::move(stream))
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
    ASSERT(_streamKeeper);
    auto stream = std::move(_streamKeeper);
    stream->readUntil("\r\n\r\n", std::bind(&HTTPConnection::onHeaders, shared_from_this(), std::placeholders::_1));
}

void HTTPConnection::write(BufferType &chunk) {
    ASSERT(getRequest(), "Request closed");
    auto stream = getStream();
    if (stream && !stream->closed()) {
        _streamKeeper.reset();
        stream->write(chunk, std::bind(&HTTPConnection::onWriteComplete, shared_from_this()));
    }
}

void HTTPConnection::finish() {
    ASSERT(getRequest(), "Request closed");
    _requestFinished = true;
    auto stream = getStream();
    _requestKeeper = getRequest();
    if (stream && !stream->writing()) {
        finishRequest();
    }
}

void HTTPConnection::onWriteComplete() {
    ASSERT(!_streamKeeper);
    auto stream = getStream();
    ASSERT(stream);
    if (stream->dying()) {
        _streamKeeper = std::move(stream);
    }
    if (_requestFinished) {
        finishRequest();
    }
}

void HTTPConnection::finishRequest() {
    ASSERT(_requestKeeper);
    bool disconnect = true;
    if (_noKeepAlive) {
        disconnect = true;
    } else {
        auto headers = _requestKeeper->getHTTPHeader();
        std::string connectionHeader = headers->get("Connection");
        if (_requestKeeper->supportsHTTP1_1()) {
            disconnect = connectionHeader == "close";
        } else if (headers->contain("Content-Length")
                   || _requestKeeper->getMethod() == "HEAD"
                   || _requestKeeper->getMethod() == "GET") {
            disconnect = connectionHeader != "Keep-Alive";
        }
    }
    _requestKeeper.reset();
    _requestFinished = false;
    if (disconnect) {
        auto stream = getStream();
        ASSERT(stream);
        stream->close();
        return;
    }
    start();
}

void HTTPConnection::onHeaders(BufferType &data) {
    ASSERT(!_streamKeeper);
    auto stream = getStream();
    ASSERT(stream);
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    try {
        const char *content = boost::asio::buffer_cast<const char *>(data);
        size_t length = boost::asio::buffer_size(data);
        const char *eol = StrNStr(content, length, "\r\n");
        std::string startLine;
        if (eol) {
            startLine.assign(content, eol);
        } else {
            startLine.assign(content, length);
        }
        StringVector requestHeaders = String::split(startLine);
        if (requestHeaders.size() != 3) {
            throw _BadRequestException("Malformed HTTP request line");
        }
        std::string method = std::move(requestHeaders[0]);
        std::string uri = std::move(requestHeaders[1]);
        std::string version = std::move(requestHeaders[2]);
        if (!boost::starts_with(version, "HTTP/")) {
            throw _BadRequestException("Malformed HTTP version in HTTP Request-Line");
        }
        HTTPHeadersPtr headers = HTTPHeaders::parse(std::string(eol, content + length));
        _requestKeeper = std::make_shared<HTTPServerRequest>(shared_from_this(), std::move(method), std::move(uri),
                                                             std::move(version), std::move(headers), "", _address);
        _request = _requestKeeper;
        auto requestHeader = _requestKeeper->getHTTPHeader();
        std::string contentLengthValue = requestHeader->get("Content-Length");
        if (!contentLengthValue.empty()) {
            size_t contentLength = (size_t) std::stoi(contentLengthValue);
            if (contentLength > stream->getMaxBufferSize()) {
                throw _BadRequestException("Content-Length too long");
            }
            if (requestHeader->get("Expect") == "100-continue") {
                std::string continueLine("HTTP/1.1 100 (Continue)\r\n\r\n");
                BufferType continueLineBuffer(continueLine.c_str(), continueLine.length());
                stream->write(continueLineBuffer);
            }
            _streamKeeper.reset();
            stream->readBytes(contentLength, std::bind(&HTTPConnection::onRequestBody, shared_from_this(),
                                                       std::placeholders::_1));
            return;
        }
        _requestKeeper->setConnection(shared_from_this());
        _requestCallback(std::move(_requestKeeper));
    } catch (_BadRequestException &e) {
        Log::info("Malformed HTTP request from %s: %s", _address.c_str(), e.what());
        stream->close();
    }
}

void HTTPConnection::onRequestBody(BufferType &data) {
    ASSERT(!_streamKeeper);
    auto stream = getStream();
    ASSERT(stream);
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    ASSERT(_requestKeeper);
    const char *content = boost::asio::buffer_cast<const char *>(data);
    size_t length = boost::asio::buffer_size(data);
    _requestKeeper->setBody(content, length);
    auto requestHeader = _requestKeeper->getHTTPHeader();
    std::string contentType = requestHeader->get("Content-Type");
    if (_requestKeeper->getMethod() == "POST" || _requestKeeper->getMethod() == "PUT") {
        if (boost::starts_with(contentType, "application/x-www-form-urlencoded")) {
            auto arguments = URLParse::parseQS(_requestKeeper->getBody(), false);
            for (auto &nv: arguments) {
                if (!nv.second.empty()) {
                    _requestKeeper->addArguments(std::move(nv.first), std::move(nv.second));
                }
            }
        } else if (boost::starts_with(contentType, "multipart/form-data")) {
            StringVector fields = String::split(contentType, ';');
            bool found = false;
            size_t pos;
            std::string k, v;
            for (auto &field: fields) {
                boost::trim(field);
                pos = field.find('=');
                if (pos != std::string::npos) {
                    k = field.substr(0, pos);
                    v = field.substr(pos + 1);
                    if (k == "boundary" && !v.empty()) {
                        parseMimeBody(std::move(v), {content, length});
                        found = true;
                        break;
                    }
                } else {
                    ThrowException(ValueError, "need more than 3 values to unpack");
                }
            }
            if (!found) {
                Log::warn("Invalid multipart/form-data");
            }
        }
    }
    _requestKeeper->setConnection(shared_from_this());
    _requestCallback(std::move(_requestKeeper));
}

void HTTPConnection::parseMimeBody(std::string boundary, std::string data) {
    ASSERT(_requestKeeper);
    if (boost::starts_with(boundary, "\"") && boost::ends_with(boundary, "\"")) {
        if (boundary.length() == 1) {
            boundary.clear();
        } else {
            boundary = boundary.substr(1, boundary.length() - 2);
        }
    }
    size_t footerLength;
    if (boost::ends_with(data, "\r\n")) {
        footerLength = boundary.length() + 6;
    } else {
        footerLength = boundary.length() + 4;
    }
    if (data.length() <= footerLength) {
        return;
    }
    std::string sep("--" + boundary + "\r\n");
    StringVector parts = String::split(data, sep), nameParts;
    std::string nameHeader, value, name, nameValue, ctype;
    HTTPHeadersPtr header;
    size_t eoh, eon;
    StringMap nameValues;
    decltype(nameValues.begin()) nameIter, fileNameIter;
    for (auto &part: parts) {
        if (part.empty()) {
            continue;
        }
        eoh = part.find("\r\n");
        if (eoh == std::string::npos) {
            Log::warn("multipart/form-data missing headers");
            continue;
        }
        header = HTTPHeaders::parse(part.substr(0, eoh));
        nameHeader = header->get("Content-Disposition");
        if (!boost::starts_with(nameHeader, "form-data;") || !boost::ends_with(part, "\r\n")) {
            Log::warn("Invalid multipart/form-data");
            continue;
        }
        if (part.length() <= eoh + 6) {
            value.clear();
        } else {
            value = part.substr(eoh + 4, part.length() - eoh - 6);
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
            ctype = header->get("Content-Type", "application/unknown");
            _requestKeeper->addFile(std::move(name), HTTPFile(std::move(fileNameIter->second),
                                                              std::move(ctype),
                                                              std::move(value)));
        } else {
            _requestKeeper->addArgument(std::move(name), std::move(value));
        }
    }
}


HTTPServerRequest::HTTPServerRequest(HTTPConnectionPtr connection,
                                     std::string method,
                                     std::string uri,
                                     std::string version,
                                     HTTPHeadersPtr &&headers,
                                     std::string body,
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
        , _startTime(ClockType::now())
        , _finishTime(TimePointType::min()) {
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

void HTTPServerRequest::write(const char *chunk, size_t length) {
    ASSERT(_connection);
    BufferType buffer(chunk, length);
    _connection->write(buffer);
}

void HTTPServerRequest::finish() {
    ASSERT(_connection);
    auto connection = std::move(_connection);
    connection->finish();
    _finishTime = std::chrono::steady_clock::now();
}

float HTTPServerRequest::requestTime() const {
    std::chrono::milliseconds elapse;
    if (_finishTime == TimePointType::min()) {
        elapse = std::chrono::duration_cast<std::chrono::milliseconds>(ClockType::now() - _startTime);
    } else {
        elapse = std::chrono::duration_cast<std::chrono::milliseconds>(_finishTime - _startTime);
    }
    return elapse.count() / 1000 + elapse.count() % 1000 / 1000.0f;
}

//const SSLOption* HTTPRequest::getSSLCertificate() const {
//    auto stream = _connection->getStream();
//    auto sslStream = std::dynamic_pointer_cast<SSLIOStream>(stream);
//    if (sslStream) {
//
//    } else {
//        return nullptr;
//    }
//}

void HTTPServerRequest::addArgument(std::string name, std::string value) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        iter->second.emplace_back(std::move(value));
    } else {
        _arguments.emplace(std::move(name), StringVector{std::move(value)});
    }
}

void HTTPServerRequest::addArguments(std::string name, StringVector values) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        for (auto &value: values) {
            iter->second.emplace_back(std::move(value));
        }
    } else {
        _arguments.emplace(std::move(name), std::move(values));
    }
}

void HTTPServerRequest::addFile(std::string name, HTTPFile file) {
    auto iter = _files.find(name);
    if (iter != _files.end()) {
        iter->second.emplace_back(std::move(file));
    } else {
        _files.emplace(std::move(name), std::vector<HTTPFile>{std::move(file)});
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
    argsList.emplace_back("body=" + _body);
    std::string args = boost::join(argsList, ",");
    StringVector headersList;
    _headers->getAll([&headersList](const std::string &name, const std::string &value){
         headersList.emplace_back("\"" + name + "\": \"" + value + "\"");
    });
    std::string headers = boost::join(headersList, ", ");
    return String::format("HTTPRequest(%s, headers=\"{%s}\"", args.c_str(), headers.c_str());
}
