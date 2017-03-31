//
// Created by yuwenyong on 17-3-28.
//

#include "tinycore/networking/httpserver.h"
#include "tinycore/networking/ioloop.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/utilities/urlparse.h"
#include "tinycore/utilities/string.h"
#include "tinycore/common/errors.h"


HTTPServer::HTTPServer(std::function<void(HTTPRequest *)> requestCallback,
                       bool noKeepAlive,
                       IOLoop *ioloop,
                       bool xheaders,
                       SSLOption *sslOption)
        : _requestCallback(std::move(requestCallback))
        , _noKeepAlive(noKeepAlive)
        , _ioloop(ioloop ? ioloop : IOLoop::instance())
        , _xheaders(xheaders)
        , _sslOption(sslOption)
        , _acceptor(_ioloop->getService())
        , _socket(_ioloop->getService()){

}


HTTPServer::~HTTPServer() {

}

int HTTPServer::listen(unsigned short port, const std::string &address) {
    if (_bind(port, address) < 0) {
        return -1;
    }
    return _start();
}

int HTTPServer::stop() {
    _acceptor.close();
}

int HTTPServer::_bind(unsigned short port, const std::string &address) {
    BaseIOStream::ResolverType resolver(_ioloop->getService());
    BaseIOStream::EndPointType endpoint = *resolver.resolve({address, port});
    _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen();
    return 0;
}

void HTTPServer::_doAccept() {
    _acceptor.async_accept(_socket, [this](ErrorCode ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            } else {
                // logging
            }
        } else {
            BaseIOStreamPtr stream;
            if (_sslOption) {
                stream = std::make_shared<SSLIOStream>(std::move(_socket), *_sslOption, _ioloop);
            } else {
                stream = std::make_shared<IOStream>(std::move(_socket), _ioloop);
            }
            auto connection = std::make_shared<HTTPConnection>(stream,
                                                               stream->getRemoteAddress(),
                                                               _requestCallback,
                                                               _noKeepAlive,
                                                               _xheaders);
            connection->start();
        }
        _doAccept();
    });
}


class _BadRequestException: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


HTTPConnection::HTTPConnection(BaseIOStreamPtr stream,
                               std::string address,
                               std::function<void(HTTPRequest *)> requestCallback,
                               bool noKeepAlive,
                               bool xheaders)
        : _stream(std::move(stream))
        , _address(std::move(address))
        , _requestCallback(std::move(requestCallback))
        , _noKeepAlive(noKeepAlive)
        , _xheaders(xheaders) {

}


int HTTPConnection::start() {
    auto callback = std::bind(&HTTPConnection::_onHeaders, shared_from_this(), std::placeholders::_1,
                              std::placeholders::_2);
    _stream->readUntil("\r\n\r\n", callback);
    return 0;
}

int HTTPConnection::write(const char *chunk, size_t length) {
    ASSERT(_request);
    if (_stream->isOpen()) {
        auto callback = std::bind(&HTTPConnection::_onWriteComplete, shared_from_this());
        _stream->write(ConstBufferType(chunk, length), callback);
    }
    return 0;
}


int HTTPConnection::finish() {
    ASSERT(_request);
    _requestFinished = true;
    if (!_stream->isWriting()) {
        _finishRequest();
    }
    return 0;
}

void HTTPConnection::_onWriteComplete() {
    if (_requestFinished) {
        _finishRequest();
    }
}

void HTTPConnection::_finishRequest() {
    bool disconnect = true;
    if (_noKeepAlive) {
        disconnect = true;
    } else {
        auto headers = _request->getHTTPHeader();
        std::string connectionHeader = headers->getDefault("Connection");
        if (_request->supportsHTTP11()) {
            disconnect = connectionHeader == "close";
        } else if (headers->contain("Content-Length") || _request->getMethod() == "HEAD"
                   || _request->getMethod() == "GET") {
            disconnect = connectionHeader != "Keep-Alive";
        }
    }
    _request.reset();
    _requestFinished = false;
    if (disconnect) {
        _stream->close();
        return;
    }
    start();
}

void HTTPConnection::_onHeaders(const char *data, size_t length) {
    try {
        const char *eol = strnstr(data, length, "\r\n");
        std::string startLine(data, eol);
        StringVector requestHeaders;
        requestHeaders = String::split(startLine);
        if (requestHeaders.size() != 3) {
            throw  _BadRequestException("Malformed HTTP request line");
        }
        std::string method = std::move(requestHeaders[0]);
        std::string uri = std::move(requestHeaders[1]);
        std::string version = std::move(requestHeaders[2]);
        if (!boost::starts_with(version, "HTTP")) {
            throw  _BadRequestException("Malformed HTTP version in HTTP Request-Line");
        }
        HTTPHeaderUPtr headers = HTTPHeader::parse(std::string(eol, data + length));
        _request = make_unique<HTTPRequest>(shared_from_this(), std::move(method), std::move(uri), std::move(version),
                                            std::move(headers), "", _address);
        auto requestHeader = _request->getHTTPHeader();
        std::string contentLengthValue = requestHeader->getDefault("Content-Length");
        if (!contentLengthValue.empty()) {
            size_t contentLength = (size_t)std::stoi(contentLengthValue);
            if (contentLength > _stream->getMaxBufferSize()) {
                throw _BadRequestException("Content-Length too long");
            }
            if (requestHeader->getDefault("Expect") == "100-continue") {
                std::string continueLine("HTTP/1.1 100 (Continue)\r\n\r\n");
                _stream->write(continueLine.c_str(), continueLine.size());
            }
            auto callback = std::bind(&HTTPConnection::_onRequestBody, shared_from_this(), std::placeholders::_1,
                                      std::placeholders::_2);
            _stream->readBytes(contentLength, callback);
            return;
        }
        _requestCallback(_request.get());
    } catch (_BadRequestException &e) {
        _stream->close();
    }
}

void HTTPConnection::_onRequestBody(const char *data, size_t length) {
    _request->setBody(data, length);
    auto requestHeader = _request->getHTTPHeader();
    std::string contentType = requestHeader->getDefault("Content-Type");
    if (_request->getMethod() == "POST" || _request->getMethod() == "PUT") {
        if (boost::starts_with(contentType, "application/x-www-form-urlencoded")) {
            auto arguments = URLParse::parseQS(std::string(data, length));
            for (auto &nv: arguments) {
                if (!nv.second.empty()) {
                    _request->addArguments(std::move(nv.first), std::move(nv.second));
                }
            }
        } else if (boost::starts_with(contentType, "multipart/form-data")) {
            StringVector fields;
            fields = String::split(contentType, ';');
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
                        _parseMimeBody(std::move(v), {data, length});
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                //logging
            }
        }
    }
    _requestCallback(_request.get());
}

void HTTPConnection::_parseMimeBody(std::string boundary, std::string data) {
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
    std::regex sep("--" + boundary + "\r\n");
    std::regex_token_iterator<std::string::iterator> rend;
    std::regex_token_iterator<std::string::iterator> pos(data.begin(), data.end() - footerLength, sep, -1);
    std::string part, nameHeader, value, name, nameValue, ctype;
    HTTPHeaderUPtr header;
    size_t eoh, eon;
    std::map<std::string, std::string> nameValues;
    decltype(nameValues.begin()) nameIter, fileNameIter;
    StringVector nameParts;
    while (pos != rend) {
        part = *pos;
        if (part.empty()) {
            continue;
        }
        eoh = part.find("\r\n");
        if (eoh == std::string::npos) {
            //logging
            continue;
        }
        header = HTTPHeader::parse(part.substr(0, eoh));
        nameHeader = header->getDefault("Content-Disposition");
        if (!boost::starts_with(nameHeader, "form-data;") || !boost::ends_with(part, "\r\n")) {
            //logging
            continue;
        }
        if (part.length() <= eoh + 6) {
            value.clear();
        } else {
            value = part.substr(eoh + 4, part.length() - eoh - 6);
        }
        nameValues.clear();
        nameHeader = nameHeader.substr(10);
        boost::split(nameParts, nameHeader, [](char ch) {
            return ch == ';';
        });
        for (auto &namePart: nameParts) {
            boost::trim(namePart);
            eon = namePart.find('=');
            if (eon == std::string::npos) {
                continue;
            }
            name = namePart.substr(0, eon);
            nameValue = namePart.substr(eon + 1);
            boost::trim(nameValue);
            nameValues.emplace({std::move(name), std::move(nameValue)});
        }
        nameIter = nameValues.find("name");
        if (nameIter == nameValues.end()) {
            //logging
            continue;
        }
        name = nameIter->second;
        fileNameIter = nameValues.find("filename");
        if (fileNameIter != nameValues.end()) {
            ctype = header->getDefault("Content-Type", "application/unknown");
            _request->addFile(std::move(name), HTTPFile(std::move(fileNameIter->second),
                                                        std::move(ctype),
                                                        std::move(value)));
        } else {
            _request->addArgument(std::move(name), std::move(value));
        }
    }
}

HTTPRequest::HTTPRequest(HTTPConnectionPtr connection,
                         std::string method,
                         std::string uri,
                         std::string version,
                         HTTPHeaderUPtr &&headers,
                         std::string body,
                         std::string remoteIp,
                         std::string protocol,
                         std::string host,
                         RequestFiles files)
        :_method(std::move(method))
        , _uri(std::move(uri))
        , _version(std::move(version))
        , _headers(std::move(headers))
        , _body(std::move(body))
        , _files(std::move(files))
        , _connection(std::move(connection))
        , _startTime(std::chrono::steady_clock::now())
        , _finishTime(){
    if (_connection && _connection->getXHeaders()) {
        _remoteIp = _headers->getDefault("X-Forwarded-For", remoteIp);
        _remoteIp = _headers->getDefault("X-Real-Ip", _remoteIp);
        _protocol = _headers->getDefault("X-Forwarded-Proto", protocol);
        _protocol = _headers->getDefault("X-Scheme", _protocol);
        if (_protocol != "http" && _protocol != "https") {
            _protocol = "http";
        }
    } else {
        _remoteIp = std::move(remoteIp);
        if (!protocol.empty()) {
            _protocol = std::move(protocol);
        } else if (_connection && std::dynamic_pointer_cast<SSLIOStream>(_connection->getStream())) {
            _protocol = "https";
        } else {
            _protocol = "http";
        }
    }
    if (!host.empty()) {
        _host = std::move(host);
    } else {
        _host = _headers->getDefault("Host", "127.0.0.1");
    }
    std::string scheme, netloc, fragment;
    std::tie(scheme, netloc, _path, _query, fragment) = URLParse::urlSplit(_uri);
    _arguments = URLParse::parseQS(_query);
}

HTTPRequest::~HTTPRequest() {

}



int HTTPRequest::write(const char *chunk, size_t length) {
    return _connection->write(chunk, length);
}

int HTTPRequest::finish() {
    _connection->finish();
    _finishTime = std::chrono::steady_clock::now();
    return 0;
}

float HTTPRequest::requestTime() const {
    return 0.0f;
}

void HTTPRequest::addArgument(std::string name, std::string value) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        iter->second.emplace_back(std::move(value));
    } else {
        _arguments.emplace(std::move(name), {std::move(value)});
    }
}

void HTTPRequest::addArguments(std::string name, StringVector values) {
    auto iter = _arguments.find(name);
    if (iter != _arguments.end()) {
        for (auto &value: values) {
            iter->second.emplace_back(std::move(value));
        }
    } else {
        _arguments.emplace(std::move(name), std::move(values));
    }
}

void HTTPRequest::addFile(std::string name, HTTPFile file) {
    auto iter = _files.find(name);
    if (iter != _files.end()) {
        iter->second.emplace_back(std::move(file));
    } else {
        _files.emplace(std::move(name), {std::move(file)});
    }
}
