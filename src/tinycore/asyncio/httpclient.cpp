//
// Created by yuwenyong on 17-5-17.
//

#include "tinycore/asyncio/httpclient.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "tinycore/asyncio/logutil.h"
#include "tinycore/crypto/base64.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"


const char* _HTTPError::what() const noexcept {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += getTypeName();
        _what += "\n\t";
        _what += "HTTP ";
        _what += std::to_string(_code);
        _what += ": ";
        _what += std::runtime_error::what();
    }
    return _what.c_str();
}


HTTPClient::HTTPClient(IOLoop *ioloop, StringMap hostnameMapping, size_t maxBufferSize)
        : _ioloop(ioloop ? ioloop : IOLoop::current())
        , _hostnameMapping(std::move(hostnameMapping))
        , _maxBufferSize(maxBufferSize) {
#ifndef NDEBUG
    sWatcher->inc(SYS_HTTPCLIENT_COUNT);
#endif
}

HTTPClient::~HTTPClient() {
#ifndef NDEBUG
    sWatcher->dec(SYS_HTTPCLIENT_COUNT);
#endif
}

void HTTPClient::fetch(std::shared_ptr<HTTPRequest> request, CallbackType callback) {
    callback = StackContext::wrap<HTTPResponse>(std::move(callback));
    do {
        NullContext ctx;
        auto connection = _HTTPConnection::create(_ioloop, shared_from_this(), std::move(request), std::move(callback),
                                                  _maxBufferSize);
        connection->start();
    } while(false);
}


_HTTPConnection::_HTTPConnection(IOLoop *ioloop,
                                 std::shared_ptr<HTTPClient> client,
                                 std::shared_ptr<HTTPRequest> request,
                                 CallbackType callback, 
                                 size_t maxBufferSize)
        : _ioloop(ioloop)
        , _client(std::move(client))
        , _request(std::move(request))
        , _callback(std::move(callback))
        , _maxBufferSize(maxBufferSize) {
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
    std::exception_ptr error;
    try {
        ExceptionStackContext ctx(std::bind(&_HTTPConnection::handleException, shared_from_this(),
                                            std::placeholders::_1));
        prepare();
        _parsed = URLParse::urlSplit(_request->getURL());
        const std::string &scheme = _parsed.getScheme();
        if (scheme != "http" && scheme != "https") {
            ThrowException(ValueError, String::format("Unsupported url scheme: %s", _request->getURL().c_str()));
        }
        std::string netloc = _parsed.getNetloc();
        if (netloc.find('@') != std::string::npos) {
            std::string userpass;
            std::tie(userpass, std::ignore, netloc) = String::rpartition(netloc, "@");
        }
        const boost::regex hostPort(R"(^(.+):(\d+)$)");
        boost::smatch match;
        std::string host;
        unsigned short port;
        if (boost::regex_match(netloc, match, hostPort)) {
            host = match[1];
            port = (unsigned short)std::stoul(match[2]);
        } else {
            host = netloc;
            port = (unsigned short)(scheme == "https" ? 443 : 80);
        }
        const boost::regex ipv6(R"(^\[.*\]$)");
        if (boost::regex_match(host, ipv6)) {
            host = host.substr(1, host.size() - 2);
        }
        _parsedHostname = host;
        if (_client) {
            const StringMap &hostnameMapping = _client->getHostnameMapping();
            auto iter = hostnameMapping.find(host);
            if (iter != hostnameMapping.end()) {
                host = iter->second;
            }
        }
        BaseIOStream::SocketType socket(_ioloop->getService());
        if (scheme == "https") {
            SSLParams sslParams(false);
            if (_request->isValidateCert()) {
                sslParams.setVerifyMode(SSLVerifyMode::CERT_REQUIRED);
                sslParams.setCheckHost(_parsedHostname);
            } else {
                sslParams.setVerifyMode(SSLVerifyMode::CERT_NONE);
            }
            auto caCerts = _request->getCACerts();
            if (caCerts) {
                sslParams.setVerifyFile(*caCerts);
            }
            auto clientKey = _request->getClientKey();
            if (clientKey) {
                sslParams.setKeyFile(*clientKey);
            }
            auto clientCert = _request->getClientCert();
            if (clientCert) {
                sslParams.setCertFile(*clientCert);
            }
            auto sslOption = SSLOption::create(sslParams);
            _stream = SSLIOStream::create(std::move(socket), std::move(sslOption), _ioloop, _maxBufferSize);
        } else {
            _stream = IOStream::create(std::move(socket), _ioloop, _maxBufferSize);
        }
        float timeout = std::min(_request->getConnectTimeout(), _request->getRequestTimeout());
        if (timeout > 0.000001f) {
            IOLoop::TimeoutCallbackType timeoutCallback = [this]() {
                onTimeout();
            };
            _timeout = _ioloop->addTimeout(timeout, StackContext::wrap(std::move(timeoutCallback)));
        }
        if (_client) {
            _stream->setCloseCallback(std::bind(&_HTTPConnection::onClose, this));
        } else {
            auto wrapper = std::make_shared<CloseCallbackWrapper>(this);
            _stream->setCloseCallback(std::bind(&CloseCallbackWrapper::operator(), std::move(wrapper)));
        }
        _stream->connect(host, port, std::bind(&_HTTPConnection::onConnect, this));
    } catch (...) {
        error = std::current_exception();
    }
    if (error) {
        handleException(error);
    }
}

const StringSet _HTTPConnection::supportedMethods = {"GET", "HEAD", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"};

void _HTTPConnection::onTimeout() {
    _timeout.reset();
    if (_callback) {
        ThrowException(_HTTPError, 599, "Timeout");
    }
}

void _HTTPConnection::removeTimeout() {
    if (!_timeout.expired()) {
        _ioloop->removeTimeout(_timeout);
        _timeout.reset();
    }
}

void _HTTPConnection::onConnect() {
    removeTimeout();
    float requestTimeout = _request->getRequestTimeout();
    if (requestTimeout > 0.000001f) {
        IOLoop::TimeoutCallbackType timeoutCallback = [this]() {
            onTimeout();
        };
        _timeout = _ioloop->addTimeout(requestTimeout, StackContext::wrap(std::move(timeoutCallback)));
    }
    const std::string &method = _request->getMethod();
    if (supportedMethods.find(method) == supportedMethods.end() && !_request->isAllowNonstandardMethods()) {
        ThrowException(KeyError, String::format("unknown method %s", method.c_str()));
    }
    if (_request->getNetworkInterface()) {
        ThrowException(NotImplementedError, "NetworkInterface not supported");
    }
    if (_request->getProxyHost()) {
        ThrowException(NotImplementedError, "ProxyHost not supported");
    }
    if (_request->getProxyPort()) {
        ThrowException(NotImplementedError, "ProxyPort not supported");
    }
    if (_request->getProxyUserName()) {
        ThrowException(NotImplementedError, "ProxyUser not supported");
    }
    if (!_request->getProxyPassword().empty()) {
        ThrowException(NotImplementedError, "ProxyPassword not supported");
    }
    auto &headers = _request->headers();
    if (!headers.has("Connection")) {
        headers["Connection"] = "close";
    }
    if (!headers.has("Host")) {
        if (_parsed.getNetloc().find('@') != std::string::npos) {
            std::string host;
            std::tie(std::ignore, std::ignore, host) = String::rpartition(_parsed.getNetloc(), "@");
            headers["Host"] = host;
        } else {
            headers["Host"] = _parsed.getNetloc();
        }
    }
    std::string userName, password;
    if (_parsed.getUserName()) {
        userName = *_parsed.getUserName();
        password = *_parsed.getPassword();
    } else if (_request->getAuthUserName()) {
        userName = *_request->getAuthUserName();
        password = _request->getAuthPassword();
    }
    if (!userName.empty()) {
        std::string auth = userName + ":" + password;
        auth = "Basic " + Base64::b64encode(std::move(auth));
        headers["Authorization"] = auth;
    }
    auto userAgent = _request->getUserAgent();
    if (userAgent) {
        headers["User-Agent"] = *userAgent;
    }
    auto requestBody = _request->getBody();
    if (!_request->isAllowNonstandardMethods()) {
        if (method == "POST" || method == "PATCH" || method == "PUT") {
            ASSERT(requestBody != nullptr);
        } else {
            ASSERT(requestBody == nullptr);
        }
    }
    if (requestBody) {
        headers["Content-Length"] = std::to_string(requestBody->size());
    }
    if (method == "POST" && !headers.has("Content-Type")) {
        headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    if (_request->isUseGzip()) {
        headers["Accept-Encoding"] = "gzip";
    }
    const std::string &parsedPath = _parsed.getPath();
    const std::string &parsedQuery = _parsed.getQuery();
    std::string reqPath = parsedPath.empty() ? "/" : parsedPath;
    if (!parsedQuery.empty()) {
        reqPath += '?';
        reqPath += parsedQuery;
    }
    StringVector requestLines;
    requestLines.emplace_back(String::format("%s %s HTTP/1.1", method.c_str(), reqPath.c_str()));
    headers.getAll([&requestLines](const std::string &k, const std::string &v){
        std::string line = k + ": " + v;
        if (line.find('\n') != std::string::npos) {
            ThrowException(ValueError, "Newline in header: " + line);
        }
        requestLines.emplace_back(std::move(line));
    });
    std::string headerData = boost::join(requestLines, "\r\n");
    headerData += "\r\n\r\n";
    _stream->write((const Byte *)headerData.data(), headerData.size());
    if (requestBody) {
        _stream->write((const Byte *)requestBody->data(), requestBody->size());
    }
    _stream->readUntilRegex("\r?\n\r?\n", std::bind(&_HTTPConnection::onHeaders, this, std::placeholders::_1));
}

void _HTTPConnection::runCallback(HTTPResponse response) {
    if (_callback) {
        CallbackType callback(std::move(_callback));
        _callback = nullptr;
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        _ioloop->spawnCallback([callback=std::move(callback), response=std::move(response)]() {
            callback(std::move(response));
        });
#else
        _ioloop->spawnCallback(std::bind([](CallbackType &callback, HTTPResponse &response) {
            callback(std::move(response));
        }, std::move(callback), std::move(response)));
#endif
    }
}

void _HTTPConnection::handleException(std::exception_ptr error) {
    if (_callback) {
        removeTimeout();
        try {
            std::rethrow_exception(error);
        } catch (std::exception &e) {
            LOG_WARNING(gGenLog, "uncaught exception:%s", e.what());
        } catch (...) {
            LOG_WARNING(gGenLog, "unknown exception");
        }
        runCallback(HTTPResponse(_request, 599, error, TimestampClock::now() - _startTime));
        if (_stream) {
            _stream->close();
        }
    }
}

void _HTTPConnection::prepare() {

}

void _HTTPConnection::onClose() {
    if (_callback) {
        std::string message("Connection closed");
        if (_stream->getError()) {
            try {
                std::rethrow_exception(_stream->getError());
            } catch (std::exception &e) {
                message = e.what();
            }
        }
        ThrowException(_HTTPError, 599, message);
    }
}

void _HTTPConnection::handle1xx(int code) {
    _stream->readUntilRegex("\r?\n\r?\n", std::bind(&_HTTPConnection::onHeaders, this, std::placeholders::_1));
}

void _HTTPConnection::onHeaders(ByteArray data) {
    const char *content = (const char *)data.data();
    const char *eol = StrNStr(content, data.size(), "\n");
    std::string firstLine, _, headerData;
    if (eol) {
        firstLine.assign(content, eol);
        _ = "\n";
        headerData.assign(eol + 1, content + data.size());
    } else {
        firstLine.assign(content, data.size());
    }
    const boost::regex firstLinePattern("HTTP/1.[01] ([0-9]+) ([^\r]*).*");
    boost::smatch match;
    if (boost::regex_match(firstLine, match, firstLinePattern)) {
        int code = std::stoi(match[1]);
        if (code >= 100 && code < 200) {
            handle1xx(code);
            return;
        } else {
            _code = code;
            _reason = match[2];
        }
    } else {
        ThrowException(Exception, "Unexpected first line");
    }
    _headers = HTTPHeaders::parse(headerData);
    boost::optional<size_t> contentLength;
    if (_headers->has("Content-Length")) {
        if (_headers->at("Content-Length").find(',') != std::string::npos) {
            StringVector pieces = String::split(_headers->at("Content-Length"), ',');
            for (auto &piece: pieces) {
                boost::trim(piece);
            }
            for (auto &piece: pieces) {
                if (piece != pieces[0]) {
                    ThrowException(ValueError, String::format("Multiple unequal Content-Lengths: %s",
                                                              _headers->at("Content-Length").c_str()));
                }
            }
            (*_headers)["Content-Length"] = pieces[0];
        }
        contentLength = std::stoul(_headers->at("Content-Length"));
    }
    auto &headerCallback = _request->getHeaderCallback();
    if (headerCallback) {
        headerCallback(firstLine + _);
        _headers->getAll([&headerCallback](const std::string &k, const std::string &v){
            headerCallback(k + ": " + v + "\r\n");
        });
        headerCallback("\r\n");
    }
    if (_request->getMethod() == "HEAD" || _code == 304) {
        onBody({});
        return;
    }
    if ((100 <= _code && _code < 200) || _code == 204) {
        if (_headers->has("Transfer-Encoding") || (contentLength && *contentLength != 0)) {
            ThrowException(ValueError, String::format("Response with code %d should not have body", *_code));
        }
        onBody({});
        return;
    }
    if (_request->isUseGzip() && _headers->get("Content-Encoding") == "gzip") {
        _decompressor = make_unique<GzipDecompressor>();
    }
    if (_headers->get("Transfer-Encoding") == "chunked") {
        _chunks = ByteArray();
        _stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, this, std::placeholders::_1));
    } else if (contentLength) {
        _stream->readBytes(*contentLength, std::bind(&_HTTPConnection::onBody, this, std::placeholders::_1));
    } else {
        _stream->readUntilClose(std::bind(&_HTTPConnection::onBody, this, std::placeholders::_1));
    }
}

void _HTTPConnection::onBody(ByteArray data) {
    if (!_timeout.expired()) {
        _ioloop->removeTimeout(_timeout);
        _timeout.reset();
    }
    auto originalRequest = _request->getOriginalRequest();
    if (!originalRequest) {
        originalRequest = _request;
    }
    if (_request->isFollowRedirects() && _request->getMaxRedirects() > 0
        && (_code == 301 || _code == 302 || _code == 303 || _code == 307)) {
        auto newRequest= HTTPRequest::create(_request);
        std::string url = _request->getURL();
        url = URLParse::urlJoin(url, _headers->at("Location"));
        newRequest->setURL(std::move(url));
        newRequest->setMaxRedirects(_request->getMaxRedirects() - 1);
        newRequest->headers().erase("Host");
        if (_code == 302 || _code == 303) {
            newRequest->setMethod("GET");
            newRequest->setBody(boost::none);
            const std::array<std::string, 4> fields = {"Content-Length", "Content-Type", "Content-Encoding",
                                                       "Transfer-Encoding"};
            for (auto &field: fields) {
                try {
                    newRequest->headers().erase(field);
                } catch (KeyError &e) {

                }
            }
        }
        newRequest->setOriginalRequest(std::move(originalRequest));
        CallbackType callback;
        callback.swap(_callback);
        if (_client) {
            _client->fetch(std::move(newRequest), std::move(callback));
        } else {
            ThrowException(ValueError, "WebSocketClientConnection has no HTTPClient");
        }
        _stream->close();
        return;
    }
    if (_decompressor) {
        data = _decompressor->decompress(data);
        auto tail = _decompressor->flush();
        if (!tail.empty()) {
            data.insert(data.end(), tail.begin(), tail.end());
        }
    }
    std::string buffer;
    auto &streamingCallback = _request->getStreamingCallback();
    if (streamingCallback) {
        if (!_chunks) {
            streamingCallback(std::move(data));
        }
    } else {
        buffer.assign((const char*)data.data(), data.size());
    }
    HTTPResponse response(originalRequest, _code.get(), _reason, *_headers, std::move(buffer), _request->getURL(),
                          TimestampClock::now() - _startTime);
    runCallback(std::move(response));
    _stream->close();
}

void _HTTPConnection::onChunkLength(ByteArray data) {
    std::string content((const char *)data.data(), data.size());
    boost::trim(content);
    size_t length = std::stoul(content, nullptr, 16);
    if (length == 0) {
        if (_decompressor) {
            auto tail = _decompressor->flush();
            if (!tail.empty()) {
                auto &streamingCallback = _request->getStreamingCallback();
                if (streamingCallback) {
                    streamingCallback(std::move(tail));
                } else {
                    _chunks->insert(_chunks->end(), tail.begin(), tail.end());
                }
            }
            _decompressor.reset();
        }
        onBody(std::move(_chunks.get()));
    } else {
        _stream->readBytes(length + 2, std::bind(&_HTTPConnection::onChunkData, this, std::placeholders::_1));
    }
}

void _HTTPConnection::onChunkData(ByteArray data) {
    ASSERT(strncmp((const char *)data.data() + data.size() - 2, "\r\n", 2) == 0);
    data.erase(std::prev(data.end(), 2), data.end());
    ByteArray chunk(std::move(data));
    if (_decompressor) {
        chunk = _decompressor->decompress(chunk);
    }
    auto &streamingCallback = _request->getStreamingCallback();
    if (streamingCallback) {
        streamingCallback(chunk);
    } else {
        _chunks->insert(_chunks->end(), chunk.begin(), chunk.end());
    }
    _stream->readUntil("\r\n", std::bind(&_HTTPConnection::onChunkLength, this, std::placeholders::_1));
}