//
// Created by yuwenyong on 17-5-17.
//

#include "tinycore/asyncio/httpclient.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "tinycore/crypto/base64.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/httputils/urlparse.h"
#include "tinycore/logging/log.h"


HTTPClient::HTTPClient(IOLoop *ioloop, StringMap hostnameMapping)
        : _ioloop(ioloop ? ioloop : sIOLoop)
        , _hostnameMapping(std::move(hostnameMapping)) {
#ifndef NDEBUG
    sWatcher->inc(SYS_HTTPCLIENT_COUNT);
#endif
}

HTTPClient::~HTTPClient() {
#ifndef NDEBUG
    sWatcher->dec(SYS_HTTPCLIENT_COUNT);
#endif
}

void HTTPClient::fetch(std::shared_ptr<HTTPRequest> originalRequest, std::shared_ptr<HTTPRequest> request,
                       CallbackType callback) {
    auto self = shared_from_this();
    auto connection = _HTTPConnection::create(_ioloop, self, std::move(originalRequest), std::move(request),
                                              std::bind(&HTTPClient::onFetchComplete, self, callback,
                                                        std::placeholders::_1));
    connection->start();
}


_HTTPConnection::_HTTPConnection(IOLoop *ioloop,
                                 std::shared_ptr<HTTPClient> client,
                                 std::shared_ptr<HTTPRequest> originalRequest,
                                 std::shared_ptr<HTTPRequest> request,
                                 CallbackType callback)
        : _ioloop(ioloop)
        , _client(std::move(client))
        , _originalRequest(std::move(originalRequest))
        , _request(std::move(request))
        , _callback(std::move(callback)) {
    _startTime = TimestampClock::now();
#ifndef NDEBUG
    sWatcher->inc(SYS_HTTPCLIENTCONNECTION_COUNT);
#endif
}

_HTTPConnection::~_HTTPConnection() {
#ifndef NDEBUG
    sWatcher->dec(SYS_HTTPCLIENTCONNECTION_COUNT);
#endif
}


void _HTTPConnection::start() {
    try {
        std::tie(_parsedScheme, _parsedNetloc, _parsedPath, _parsedQuery, _parsedFragment) =
                URLParse::urlSplit(_request->getURL());
        std::string host;
        unsigned short port;
        auto pos = _parsedNetloc.find(':');
        if (pos != std::string::npos) {
            host = _parsedNetloc.substr(0, pos);
            port = (unsigned short)std::stoi(_parsedNetloc.substr(pos + 1));
        } else {
            host = _parsedNetloc;
            port = (unsigned short)(_parsedScheme == "https" ? 443 : 80);
        }
        const StringMap &hostnameMapping = _client->getHostnameMapping();
        auto iter = hostnameMapping.find(host);
        if (iter != hostnameMapping.end()) {
            host = iter->second;
        }
        BaseIOStream::SocketType socket(_ioloop->getService());
        if (_parsedScheme == "https") {
            std::shared_ptr<SSLOption> sslOption;
            const std::string &caCerts = _request->getCACerts();
            if (_request->isValidateCert()) {
                if (caCerts.empty()) {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_REQUIRED);
                } else {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_REQUIRED, caCerts);
                }
            } else {
                if (caCerts.empty()) {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_NONE);
                } else {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_NONE, caCerts);
                }
            }
            _stream = SSLIOStream::create(std::move(socket), std::move(sslOption), _ioloop);
        } else {
            _stream = IOStream::create(std::move(socket), _ioloop);
        }
        _streamObserver = _stream;
        auto self = shared_from_this();
        float timeout = std::min(_request->getConnectTimeout(), _request->getRequestTimeout());
        if (timeout > 0.0f) {
            _connectTimeout = _ioloop->addTimeout(timeout, std::bind(&_HTTPConnection::onTimeout, self));
        }
        std::weak_ptr<_HTTPConnection> connectionObserver = self;
        _stream->setCloseCallback([connectionObserver](){
            auto connection = connectionObserver.lock();
            if (connection) {
                connection->onClose();
            }
        });
        auto stream = std::move(_stream);
        stream->connect(host, port, std::bind(&_HTTPConnection::onConnect, self));
    } catch (std::exception &e) {
        std::string error = e.what();
        Log::warn("uncaught exception:%s", error.c_str());
        if (_callback) {
            CallbackType callback;
            callback.swap(_callback);
            callback(HTTPResponse(_request, 599, error_=error));
        }
    }
}

const StringSet _HTTPConnection::supportedMethods = {"GET", "HEAD", "POST", "PUT", "DELETE"};

void _HTTPConnection::onTimeout() {
    if (_callback) {
        CallbackType callback;
        callback.swap(_callback);
        callback(HTTPResponse(_request, 599, error_="Timeout"));
    }
    auto stream = fetchStream();
    if (stream) {
        stream->close();
    }
}

void _HTTPConnection::onConnect() {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    _ioloop->removeTimeout(_timeout);
    auto self = shared_from_this();
    float requestTimeout = _request->getRequestTimeout();
    if (requestTimeout > 0.0f) {
        _timeout = _ioloop->addTimeout(requestTimeout, std::bind(&_HTTPConnection::onTimeout, self));
    }
    const std::string &method = _request->getMethod();
    if (supportedMethods.find(method) == supportedMethods.end() && !_request->isAllowNonstandardMethods()) {
        ThrowException(KeyError, String::format("unknown method %s", method.c_str()));
    }
    if (!_request->getNetworkInterface().empty()) {
        ThrowException(NotImplementedError, "NetworkInterface not supported");
    }
    if (!_request->getProxyHost().empty()) {
        ThrowException(NotImplementedError, "ProxyHost not supported");
    }
    if (_request->getProxyPort() != 0) {
        ThrowException(NotImplementedError, "ProxyPort not supported");
    }
    if (!_request->getProxyUserName().empty()) {
        ThrowException(NotImplementedError, "ProxyUser not supported");
    }
    if (!_request->getProxyPassword().empty()) {
        ThrowException(NotImplementedError, "ProxyPassword not supported");
    }
    auto &headers = _request->headers();
    if (!headers.has("Host")) {
        headers["Host"] = _parsedNetloc;
    }
    const std::string &authUserName = _request->getAuthUserName();
    if (!authUserName.empty()) {
        std::string auth = authUserName + ":" + _request->getAuthPassword();
        auth = "Basic " + Base64::b64decode(std::move(auth));
        headers["Authorization"] = auth;
    }
    const std::string &userAgent = _request->getUserAgent();
    if (!userAgent.empty()) {
        headers["User-Agent"] = userAgent;
    }
    bool hasBody = method == "POST" || method == "PUT";
    auto requestBody = _request->getBody();
    if (hasBody) {
        ASSERT(requestBody != nullptr);
        headers["Content-Length"] = std::to_string(requestBody->size());
    } else {
        ASSERT(requestBody == nullptr);
    }
    if (method == "POST" && !headers.has("Content-Type")) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    if (_request->isUseGzip()) {
        headers["Accept-Encoding"] = "gzip";
    }
    std::string reqPath = _parsedPath.empty() ? "/" : _parsedPath;
    if (!_parsedQuery.empty()) {
        reqPath += '?';
        reqPath += _parsedQuery;
    }
    StringVector requestLines;
    requestLines.emplace_back(String::format("%s %s HTTP/1.1", method.c_str(), reqPath.c_str()));
    headers.getAll([&requestLines](const std::string &k, const std::string &v){
        requestLines.emplace_back(k + ": " + v);
    });
    std::string headerData = boost::join(requestLines, "\r\n");
    headerData += "\r\n\r\n";
    stream->write((const Byte *)headerData.data(), headerData.size());
    if (hasBody) {
        stream->write(requestBody->data(), requestBody->size());
    }
    _stream.reset();
    stream->readUntil("\r\n\r\n", std::bind(&_HTTPConnection::onHeaders, self, std::placeholders::_1,
                                            std::placeholders::_2));
}

void _HTTPConnection::onClose() {
    fprintf(stderr, "_HTTPConnection::onClose\n");
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    if (_callback) {
        CallbackType callback;
        callback.swap(_callback);
        callback(HTTPResponse(_request, 599, error_="Connection closed"));
    }
}

void _HTTPConnection::onHeaders(Byte *data, size_t length) {
    std::string headersTmp((const char *)data, length);
//    fprintf(stderr, "Onheaders:%s\n", headersTmp.c_str());
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    const char *content = (const char *)data;
    const char *eol = StrNStr(content, length, "\r\n");
    std::string firstLine, headerData;
    if (eol) {
        firstLine.assign(content, eol);
        headerData.assign(eol + 2, content + length);
    } else {
        firstLine.assign(content, length);
    }
    const boost::regex firstLinePattern("HTTP/1.[01] ([0-9]+) .*");
    boost::smatch match;
    if (boost::regex_match(firstLine, match, firstLinePattern)) {
        _code = std::stoi(match[1]);
    } else {
        ASSERT(false);
        ThrowException(Exception, "Unexpected first line");
    }
    _headers = HTTPHeaders::parse(headerData);
    auto &headerCallback = _request->getHeaderCallback();
    if (headerCallback) {
        _headers->getAll([&headerCallback](const std::string &k, const std::string &v){
            headerCallback(k + ": " + v + "\r\n");
        });
    }
    if (_request->isUseGzip() && _headers->get("Content-Encoding") == "gzip") {
        _decompressor = make_unique<DecompressObj>(16 + Zlib::maxWBits);
    }
    if (_headers->get("Transfer-Encoding") == "chunked") {
        _chunks = ByteArray();
        _stream.reset();
        stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, shared_from_this(),
                                            std::placeholders::_1, std::placeholders::_2));
    } else if (_headers->has("Content-Length")) {
        std::string __length = _headers->at("Content-Length");
        size_t contentLength = std::stoul(_headers->at("Content-Length"));
        _stream.reset();
        stream->readBytes(contentLength, std::bind(&_HTTPConnection::onBody, shared_from_this(),
                                                   std::placeholders::_1, std::placeholders::_2));
    } else {
        std::string error("No Content-length or chunked encoding,don't know how to read ");
        error += _request->getURL();
        ThrowException(Exception, error);
    }
}

void _HTTPConnection::onBody(Byte *data, size_t length) {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    _ioloop->removeTimeout(_timeout);
    ByteArray body;
    if (_decompressor) {
        body = _decompressor->decompress(data, length);
    } else {
        body.assign(data, data + length);
    }
    ByteArray buffer;
    auto &streamCallback = _request->getStreamCallback();
    if (streamCallback) {
        if (!_chunks) {
            streamCallback(buffer);
        }
    } else {
        buffer = std::move(body);
    }
    if (_request->isFollowRedirects() && _request->getMaxRedirects() > 0 && (_code == 301 || _code == 302)) {
        auto newRequest= HTTPRequest::create(_request);
        std::string url = _request->getURL();
        url = URLParse::urlJoin(url, _headers->at("Location"));
        newRequest->setURL(std::move(url));
        newRequest->decreaseRedirects();
        newRequest->headers().erase("Host");
        CallbackType callback;
        callback.swap(_callback);
        _client->fetch(_originalRequest, std::move(newRequest), callback);
        return;
    }
    HTTPResponse response(_originalRequest, _code.get(), headers_=*_headers, body_=buffer,
                          effectiveURL_=_request->getURL());
    CallbackType callback;
    callback.swap(_callback);
    callback(response);
}

void _HTTPConnection::onChunkLength(Byte *data, size_t length) {
    std::string content((const char *)data, (const char *)data + length);
    boost::trim(content);
    size_t chunkLength = std::stoul(content, nullptr, 16);
    if (chunkLength == 0) {
        _decompressor.reset();
        onBody(_chunks->data(), _chunks->size());
    } else {
        auto stream = fetchStream();
        stream->readBytes(chunkLength + 2, std::bind(&_HTTPConnection::onChunkData, shared_from_this(),
                                                     std::placeholders::_1, std::placeholders::_2));
    }
}

void _HTTPConnection::onChunkData(Byte *data, size_t length) {
    ASSERT(strncmp((const char *)data + length - 2, "\r\n", 2) == 0);
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    ByteArray chunk(data, data + length - 2);
    if (_decompressor) {
        chunk = _decompressor->decompress(chunk);
    }
    auto &streamCallback = _request->getStreamCallback();
    if (streamCallback) {
        streamCallback(chunk);
    } else {
        _chunks->resize(_chunks->size() + chunk.size());
        memcpy(_chunks->data() + _chunks->size() - chunk.size(), chunk.data(), chunk.size());
    }
    _stream.reset();
    stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, shared_from_this(),
                                        std::placeholders::_1, std::placeholders::_2));
}