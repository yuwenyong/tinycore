//
// Created by yuwenyong on 17-5-17.
//

#include "tinycore/networking/httpclient.h"
#include "tinycore/logging/log.h"
#include "tinycore/utilities/urlparse.h"


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
        std::tie(_parsedScheme, _parsedNetloc, _parsedURL, _parsedQuery, _parsedFragment) =
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
            if (_request->getCACerts().empty()) {
                sslOption = SSLOption::createClientSide();
            } else {
                sslOption = SSLOption::createClientSide(_request->getCACerts());
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
    _ioloop->removeTimeout(_timeout);
    auto self = shared_from_this();
    float requestTimeout = _request->getRequestTimeout();
    if (requestTimeout > 0.0f) {
        _timeout = _ioloop->addTimeout(requestTimeout, std::bind(&_HTTPConnection::onTimeout, self));
    }
    const std::string &method = _request->getMethod();
    if (supportedMethods.find(method) == supportedMethods.end() && !_request->getAllowNonstandardMethods()) {
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
}

void _HTTPConnection::onClose() {
    if (_callback) {
        CallbackType callback(std::move(_callback));
        callback(HTTPResponse(_request, 599, error_="Connection closed"));
    }
}