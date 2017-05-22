//
// Created by yuwenyong on 17-5-17.
//

#include "tinycore/networking/httpclient.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "tinycore/logging/log.h"
#include "tinycore/httputils/urlparse.h"
#include "tinycore/crypto/base64.h"
#include "tinycore/debugging/trace.h"


HTTPClient::HTTPClient(IOLoop *ioloop, StringMap hostnameMapping)
        : _ioloop(ioloop ? ioloop : sIOLoop)
        , _hostnameMapping(std::move(hostnameMapping)) {

}

void HTTPClient::fetch(HTTPRequestPtr originalRequest, HTTPRequestPtr request, CallbackType callback) {
    auto self = shared_from_this();
    auto connection = _HTTPConnection::create(_ioloop, self, std::move(originalRequest), std::move(request),
                                              std::bind(&HTTPClient::onFetchComplete, self, callback,
                                                        std::placeholders::_1));
    connection->start();
}


_HTTPConnection::_HTTPConnection(IOLoop *ioloop,
                                 HTTPClientPtr client,
                                 HTTPRequestPtr originalRequest,
                                 HTTPRequestPtr request,
                                 CallbackType callback)
        : _ioloop(ioloop)
        , _client(std::move(client))
        , _originalRequest(std::move(originalRequest))
        , _request(std::move(request))
        , _callback(std::move(callback)) {
    _startTime = TimestampClock::now();
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
            SSLOptionPtr sslOption;
            const std::string &caCerts = _request->getCACerts();
            if (_request->isValidateCert()) {
                if (caCerts.empty()) {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_REQUIRED);
                } else {
                    sslOption = SSLOption::createClientSide(SSLVerifyMode::CERT_REQUIRED, caCerts);
                }
            } else {
                if (caCerts.empty()) {
                    sslOption = SSLOption::createClientSide();
                } else {
                    sslOption = SSLOption::createClientSide(caCerts);
                }
            }
            _streamKeeper = std::make_shared<SSLIOStream>(std::move(socket), std::move(sslOption), _ioloop);
        } else {
            _streamKeeper = std::make_shared<IOStream>(std::move(socket), _ioloop);
        }
        _stream = _streamKeeper;
        auto self = shared_from_this();
        float timeout = std::min(_request->getConnectTimeout(), _request->getRequestTimeout());
        if (timeout > 0.0f) {
            _connectTimeout = _ioloop->addTimeout(timeout, std::bind(&_HTTPConnection::onTimeout, self));
        }
        _HTTPConnectionWPtr connection = shared_from_this();
        _streamKeeper->setCloseCallback([connection](){
            auto connectionKeeper = connection.lock();
            if (connectionKeeper) {
                connectionKeeper->onClose();
            }
        });
        auto stream = std::move(_streamKeeper);
        stream->connect(host, port, std::bind(&_HTTPConnection::onConnect, self));
    } catch (std::exception &e) {
        std::string error = e.what();
        Log::warn("uncaught exception:%s", error.c_str());
        if (_callback) {
            CallbackType callback(std::move(_callback));
            callback(HTTPResponse(_request, 599, error_=error));
        }
    }
}

const StringSet _HTTPConnection::supportedMethods = {"GET", "HEAD", "POST", "PUT", "DELETE"};

void _HTTPConnection::onTimeout() {
    if (_callback) {
        CallbackType callback(std::move(_callback));
        callback(HTTPResponse(_request, 599, error_="Timeout"));
    }
    auto stream = getStream();
    if (stream) {
        stream->close();
    }
}

void _HTTPConnection::onConnect() {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
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
    if (!headers.contain("Host")) {
        headers.setItem("Host", _parsedNetloc);
    }
    const std::string &authUserName = _request->getAuthUserName();
    if (!authUserName.empty()) {
        std::string auth = authUserName + ":" + _request->getAuthPassword();
        auth = "Basic " + Base64::b64decode(std::move(auth));
        headers.setItem("Authorization", auth);
    }
    const std::string &userAgent = _request->getUserAgent();
    if (!userAgent.empty()) {
        headers.setItem("User-Agent", userAgent);
    }
    bool hasBody = method == "POST" || method == "PUT";
    auto requestBody = _request->getBody();
    if (hasBody) {
        ASSERT(requestBody != nullptr);
        headers.setItem("Content-Length", std::to_string(requestBody->size()));
    } else {
        ASSERT(requestBody == nullptr);
    }
    if (method == "POST" && !headers.contain("Content-Type")) {
        headers.setItem("Content-Type", "application/x-www-form-urlencoded");
    }
    if (_request->isUseGzip()) {
        headers.setItem("Accept-Encoding", "gzip");
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
    BufferType headerBuffer(headerData.c_str(), headerData.size());
    stream->write(headerBuffer);
    if (hasBody) {
        BufferType bodyBuffer(requestBody->data(), requestBody->size());
        stream->write(bodyBuffer);
    }
    _streamKeeper.reset();
    stream->readUntil("\r\n\r\n", std::bind(&_HTTPConnection::onHeaders, self, std::placeholders::_1));
}

void _HTTPConnection::onClose() {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    if (_callback) {
        CallbackType callback(std::move(_callback));
        callback(HTTPResponse(_request, 599, error_="Connection closed"));
    }
}

void _HTTPConnection::onHeaders(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    const char *content = boost::asio::buffer_cast<const char *>(data);
    size_t length = boost::asio::buffer_size(data);
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
        _streamKeeper.reset();
        stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, shared_from_this(),
                                            std::placeholders::_1));
    } else if (_headers->contain("Content-Length")) {
        std::string __length = _headers->getItem("Content-Length");
        size_t contentLength = std::stoul(_headers->getItem("Content-Length"));
        _streamKeeper.reset();
        stream->readBytes(contentLength, std::bind(&_HTTPConnection::onBody, shared_from_this(),
                                                   std::placeholders::_1));
    } else {
        std::string error("No Content-length or chunked encoding,don't know how to read ");
        error += _request->getURL();
        ThrowException(Exception, error);
    }
}

void _HTTPConnection::onBody(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    const char *content = boost::asio::buffer_cast<const char *>(data);
    size_t length = boost::asio::buffer_size(data);
    _ioloop->removeTimeout(_timeout);
    ByteArray body;
    if (_decompressor) {
        body = _decompressor->decompress((const Byte *)content, length);
    } else {
        body.assign((const Byte *)content, (const Byte *)content + length);
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
        url = URLParse::urlJoin(url, _headers->getItem("Location"));
        newRequest->setURL(std::move(url));
        newRequest->decreaseRedirects();
        newRequest->headers().delItem("Host");
        _client->fetch(_originalRequest, std::move(newRequest), std::move(_callback));
        return;
    }
    HTTPResponse response(_originalRequest, _code.get(), headers_=*_headers, body_=buffer,
                          effectiveURL_=_request->getURL());
    CallbackType callback(std::move(_callback));
    callback(response);
}

void _HTTPConnection::onChunkLength(BufferType &data) {
    const char *dataPtr = boost::asio::buffer_cast<const char *>(data);
    size_t dataSize = boost::asio::buffer_size(data);
    std::string content(dataPtr, dataPtr + dataSize);
    boost::trim(content);
    size_t length = std::stoul(content, nullptr, 16);
    if (length == 0) {
        _decompressor.reset();
        BufferType body(_chunks->data(), _chunks->size());
        onBody(body);
    } else {
        auto stream = getStream();
        stream->readBytes(length + 2, std::bind(&_HTTPConnection::onChunkData, shared_from_this(),
                                                std::placeholders::_1));
    }
}

void _HTTPConnection::onChunkData(BufferType &data) {
    const char *content = boost::asio::buffer_cast<const char *>(data);
    size_t length = boost::asio::buffer_size(data);
    ASSERT(strncmp(content + length - 2, "\r\n", 2) == 0);
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    ByteArray chunk((const Byte *)content, (const Byte *)content + length - 2);
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
    _streamKeeper.reset();
    stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, shared_from_this(),
                                        std::placeholders::_1));
}