//
// Created by yuwenyong on 17-5-5.
//

#include "tinycore/asyncio/websocket.h"
#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>
#include "tinycore/asyncio/logutil.h"
#include "tinycore/common/random.h"
#include "tinycore/crypto/base64.h"
#include "tinycore/crypto/hashlib.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/utilities/string.h"


WebSocketHandler::WebSocketHandler(Application *application, std::shared_ptr<HTTPServerRequest> request)
        : RequestHandler(application, request) {
    _stream = request->getConnection()->getStream();
#ifndef NDEBUG
    sWatcher->inc(SYS_WEBSOCKETHANDLER_COUNT);
#endif
}

WebSocketHandler::~WebSocketHandler() {
#ifndef NDEBUG
    sWatcher->dec(SYS_WEBSOCKETHANDLER_COUNT);
#endif
}

void WebSocketHandler::writeMessage(const Byte *message, size_t length, bool binary) {
    _wsConnection->writeMessage(message, length, binary);
}

boost::optional<std::string> WebSocketHandler::selectSubProtocol(const StringVector &subProtocols) const {
    return boost::none;
}

void WebSocketHandler::onOpen(const StringVector &args) {

}

void WebSocketHandler::ping(const Byte *data, size_t length) {
    _wsConnection->writePing(data, length);
}

void WebSocketHandler::onPong(ByteArray data) {

}

void WebSocketHandler::onClose() {

}

void WebSocketHandler::close() {
    if (_wsConnection) {
        _wsConnection->close();
    }
}

bool WebSocketHandler::allowDraft76() const {
    return false;
}

void WebSocketHandler::onConnectionClose() {
    if (_wsConnection) {
        _wsConnection->onConnectionClose();
        _wsConnection.reset();
        onClose();
    }
}

void WebSocketHandler::execute(TransformsType &transforms, StringVector args) {
    _openArgs = std::move(args);
    if (_request->getMethod() != "GET") {
        const char *error = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        _stream->write((const Byte *)error, strlen(error));
        _stream->close();
        return;
    }
    if (boost::to_lower_copy(_request->getHTTPHeaders()->get("Upgrade", "")) != "websocket") {
        const char *error = "HTTP/1.1 400 Bad Request\r\n\r\nCan \"Upgrade\" only to \"WebSocket\".";
        _stream->write((const Byte *)error, strlen(error));
        _stream->close();
        return;
    }
    auto headers = _request->getHTTPHeaders();
    auto connection = String::split(headers->get("Connection", ""), ',');
    for (auto &v: connection) {
        boost::to_lower(v);
    }
    if (std::find(connection.begin(), connection.end(), "upgrade") == connection.end()) {
        const char *error = "HTTP/1.1 400 Bad Request\r\n\r\n\"Connection\" must be \"Upgrade\".";
        _stream->write((const Byte *)error, strlen(error));
        _stream->close();
        return;
    }
    std::string webSocketVersion = headers->get("Sec-WebSocket-Version");
    if (webSocketVersion == "7" || webSocketVersion == "8" || webSocketVersion == "13") {
        _wsConnection = make_unique<WebSocketProtocol13>(this);
        _wsConnection->acceptConnection();
    } else if (allowDraft76() && !headers->has("Sec-WebSocket-Version")) {
        _wsConnection = make_unique<WebSocketProtocol76>(this);
        _wsConnection->acceptConnection();
    } else {
        const char *error = "HTTP/1.1 426 Upgrade Required\r\nSec-WebSocket-Version: 8\r\n\r\n";
        _stream->write((const Byte *)error, strlen(error));
        _stream->close();
    }
}

#define WEBSOCKET_ASYNC() try
#define WEBSOCKET_ASYNC_END catch (std::exception &e) { \
    LOG_ERROR(gAppLog, "Uncaught exception %s in %s", e.what(), _request->getPath().c_str()); \
    abort(); \
} catch(...) { \
    LOG_ERROR(gAppLog, "Unknown exception in %s", _request->getPath().c_str()); \
    abort(); \
}

void WebSocketProtocol76::acceptConnection() {
    try {
        handleWebSocketHeaders();
    } catch (ValueError &e) {
        LOG_DEBUG(gGenLog, "Malformed WebSocket request received");
        abort();
        return;
    }
    std::string scheme = _handler->getWebSocketScheme();
    std::string subProtocolHeader;
    if (_request->getHTTPHeaders()->has("Sec-WebSocket-Protocol")) {
        auto &subProtocol = _request->getHTTPHeaders()->at("Sec-WebSocket-Protocol");
        auto selected = _handler->selectSubProtocol({subProtocol});
        if (selected) {
            ASSERT(selected == subProtocol);
            subProtocolHeader = String::format("Sec-WebSocket-Protocol: %s\r\n", selected->c_str());
        }
    }

    std::string origin = _request->getHTTPHeaders()->at("Origin");
    std::string initial = String::format("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Server: %s\r\n"
                                                 "Sec-WebSocket-Origin: %s\r\n"
                                                 "Sec-WebSocket-Location: %s://%s%s\r\n"
                                                 "%s\r\n",
                                         TINYCORE_VER, origin.c_str(), scheme.c_str(), _request->getHost().c_str(),
                                         _request->getURI().c_str(), subProtocolHeader.c_str());
    _stream->write((const Byte *)initial.data(), initial.size());
    _stream->readBytes(8, [this, handler=_handler->shared_from_this()](ByteArray data) {
        handleChallenge(std::move(data));
    });
}

std::string WebSocketProtocol76::challengeResponse(const std::string &challenge) const {
    auto headers = _request->getHTTPHeaders();
    std::string key1, key2, part1, part2;
    key1 = headers->get("Sec-Websocket-Key1");
    key2 = headers->get("Sec-Websocket-Key2");
    try {
        part1 = calculatePart(key1);
        part2 = calculatePart(key2);
    } catch (ValueError &e) {
        ThrowException(ValueError, "Invalid Keys/Challenge");
    }
    return generateChallengeResponse(key1, key2, challenge);
}

void WebSocketProtocol76::writeMessage(const Byte *message, size_t length, bool binary) {
    if (binary) {
        ThrowException(ValueError, "Binary messages not supported by this version of websockets");
    }
    ByteArray buffer;
    buffer.push_back('\x00');
    buffer.insert(buffer.end(), message, message + length);
    buffer.push_back('\xff');
    _stream->write(buffer.data(), buffer.size());
}

void WebSocketProtocol76::writePing(const Byte *data, size_t length) {
    ThrowException(ValueError, "Ping messages not supported by this version of websockets");
}

void WebSocketProtocol76::close() {
    if (!_serverTerminated) {
        if (!_stream->closed()) {
            const char data[] = {'\xff', '\x00'};
            _stream->write((const Byte *)data, sizeof(data));
        }
        _serverTerminated = true;
    }
    if (_clientTerminated) {
        if (!_waiting.expired()) {
            _stream->ioloop()->removeTimeout(_waiting);
        }
        _waiting.reset();
        _stream->close();
    } else if (_waiting.expired()) {
        _waiting = _stream->ioloop()->addTimeout(5.0f, [this, handler=_handler->shared_from_this()]() {
            abort();
        });
    }
}

void WebSocketProtocol76::handleChallenge(ByteArray data) {
    std::string challengeResp;
    try {
        std::string challenge((const char *)data.data(), data.size());
        challengeResp = challengeResponse(challenge);
    } catch (ValueError &e) {
        LOG_DEBUG(gGenLog, "Malformed key data in WebSocket request");
        abort();
        return;
    }
    writeResponse(challengeResp);
}

void WebSocketProtocol76::writeResponse(const std::string &challenge) {
    _stream->write((const Byte *)challenge.data(), challenge.size());
    WEBSOCKET_ASYNC() {
        _handler->open();
    } WEBSOCKET_ASYNC_END
    receiveMessage();
}

void WebSocketProtocol76::handleWebSocketHeaders() const {
    auto headers = _request->getHTTPHeaders();
    const StringVector fields = {"Origin", "Host", "Sec-Websocket-Key1", "Sec-Websocket-Key2"};
    for (auto &field: fields) {
        if (headers->get(field).empty()) {
            ThrowException(ValueError, "Missing/Invalid WebSocket headers");
        }
    }
}

std::string WebSocketProtocol76::calculatePart(const std::string &key) const {
    std::string number = String::filter(key, boost::is_digit());
    size_t spaces = String::count(key, boost::is_space());
    if (number.empty() || spaces == 0) {
        ThrowException(ValueError, "");
    }
    unsigned int keyNumber = (unsigned int)(std::stoul(number) / spaces);
    boost::endian::native_to_big_inplace(keyNumber);
    return std::string((char *)&keyNumber, 4);
}

std::string WebSocketProtocol76::generateChallengeResponse(const std::string &part1, const std::string &part2,
                                                           const std::string &part3) const {
    MD5Object m;
    m.update(part1);
    m.update(part2);
    m.update(part3);
    return m.digest();
}

void WebSocketProtocol76::onFrameType(ByteArray data) {
    unsigned char frameType = data[0];
    if (frameType == 0x00) {
        std::string delimiter('\xff', 1);;
        _stream->readUntil(std::move(delimiter), [this, handler=_handler->shared_from_this()](ByteArray data) {
            onEndDelimiter(std::move(data));
        });
    } else if (frameType == 0xff) {
        _stream->readBytes(1, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onLengthIndicator(std::move(data));
        });
    } else {
        abort();
    }
}

void WebSocketProtocol76::onEndDelimiter(ByteArray data) {
    if (!_clientTerminated) {
        WEBSOCKET_ASYNC() {
            data.pop_back();
            _handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END
    }
    if (!_clientTerminated) {
        receiveMessage();
    }
}

void WebSocketProtocol76::onLengthIndicator(ByteArray data) {
    unsigned char byte = data[0];
    if (byte != 0x00) {
        abort();
        return;
    }
    _clientTerminated = true;
    close();
}


void WebSocketProtocol13::acceptConnection() {
    try {
        handleWebSocketHeaders();
        _acceptConnection();
    } catch (ValueError &e) {
        LOG_DEBUG(gGenLog, "Malformed WebSocket request received");
        abort();
        return;
    }
}

void WebSocketProtocol13::writeMessage(const Byte *message, size_t length, bool binary) {
    Byte opcode = (Byte)(binary ? 0x02 : 0x01);
    try {
        writeFrame(true, opcode, message, length);
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketProtocol13::writePing(const Byte *data, size_t length) {
    writeFrame(true, 0x09, data, length);
}

void WebSocketProtocol13::close() {
    if (!_serverTerminated) {
        if (!_stream->closed()) {
            writeFrame(true, 0x08, nullptr, 0);
        }
        _serverTerminated = true;
    }
    if (_clientTerminated) {
        if (!_waiting.expired()) {
            _stream->ioloop()->removeTimeout(_waiting);
            _waiting.reset();
        }
        _stream->close();
    } else if (_waiting.expired()) {
        _waiting = _stream->ioloop()->addTimeout(5.0f, [this, handler=_handler->shared_from_this()]() {
            abort();
        });
    }
}

std::string WebSocketProtocol13::computeAcceptValue(const std::string &key) {
    SHA1Object sha1;
    sha1.update(key);
    sha1.update("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return Base64::b64encode(sha1.digest());
}

void WebSocketProtocol13::handleWebSocketHeaders() const {
    auto headers = _request->getHTTPHeaders();
    const StringVector fields = {"Host", "Sec-Websocket-Key", "Sec-Websocket-Version"};
    for (auto &field: fields) {
        if (headers->get(field).empty()) {
            ThrowException(ValueError, "Missing/Invalid WebSocket headers");
        }
    }
}

void WebSocketProtocol13::_acceptConnection() {
    std::string subProtocolHeader;
    auto subProtocols = String::split(_request->getHTTPHeaders()->get("Sec-WebSocket-Protocol", ""), ',');
    for (auto &subProtocol: subProtocols) {
        boost::trim(subProtocol);
    }
    if (!subProtocols.empty()) {
        auto selected = _handler->selectSubProtocol(subProtocols);
        if (selected) {
            ASSERT(std::find(subProtocols.begin(), subProtocols.end(), *selected) != subProtocols.end());
            subProtocolHeader = String::format("Sec-WebSocket-Protocol: %s\r\n", selected->c_str());
        }
    }
    std::string challengeResp = challengeResponse();
    std::string initial = String::format("HTTP/1.1 101 Switching Protocols\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Sec-WebSocket-Accept: %s\r\n"
                                                 "%s\r\n", challengeResp.c_str(), subProtocolHeader.c_str());
    _stream->write((const Byte *)initial.data(), initial.size());
    WEBSOCKET_ASYNC() {
        _handler->open();
    } WEBSOCKET_ASYNC_END
    receiveFrame();
}

void WebSocketProtocol13::writeFrame(bool fin, Byte opcode, const Byte *data, size_t length) {
    auto finbit = (Byte)(fin ? 0x80 : 0x00);
    ByteArray frame;
    frame.push_back(finbit | opcode);
    auto maskBit = (Byte)(_maskOutgoing ? 0x80 : 0x00);
    if (length < 126) {
        frame.push_back((Byte)length | maskBit);
    } else if (length <= 0xFFFF) {
        frame.push_back((Byte)126 | maskBit);
        uint16_t frameLength = (uint16_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    } else {
        frame.push_back((Byte)127 | maskBit);
        uint64_t frameLength = (uint64_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    }
    if (length) {
        if (_maskOutgoing) {
            std::array<Byte, 4> mask;
            Random::randBytes(mask);
            frame.insert(frame.end(), mask.begin(), mask.end());
            frame.insert(frame.end(), data, data + length);
            applyMask(mask, frame.data() + frame.size() - length, length);
        } else {
            frame.insert(frame.end(), data, data + length);
        }
    }
    _stream->write(frame.data(), frame.size());
}

void WebSocketProtocol13::onFrameStart(ByteArray data) {
    Byte header = data[0], payloadLen = data[1];
    _finalFrame = (header & 0x80) != 0;
    Byte reservedBits = (Byte)(header & 0x70);
    _frameOpcode = (Byte)(header & 0x0f);
    _frameOpcodeIsControl = (_frameOpcode & 0x8) != 0;
    if (reservedBits != 0) {
        abort();
        return;
    }
    _maskedFrame = (payloadLen & 0x80) != 0;
    payloadLen &= 0x7f;
    if (_frameOpcodeIsControl && payloadLen >= 126) {
        abort();
        return;
    }
    try {
        if (payloadLen < 126) {
            _frameLength = payloadLen;
            if (_maskedFrame) {
                _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
                    onMaskingKey(std::move(data));
                });
            } else {
                _stream->readBytes(_frameLength, [this, handler=_handler->shared_from_this()](ByteArray data) {
                    onFrameData(std::move(data));
                });
            }
        } else if (payloadLen == 126) {
            _stream->readBytes(2, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onFrameLength16(std::move(data));
            });
        } else if (payloadLen == 127) {
            _stream->readBytes(8, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onFrameLength64(std::move(data));
            });
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketProtocol13::onFrameLength16(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(unsigned short *)data.data());
    try {
        if (_maskedFrame) {
            _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onMaskingKey(std::move(data));
            });
        } else {
            _stream->readBytes(_frameLength, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onFrameData(std::move(data));
            });
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketProtocol13::onFrameLength64(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(uint64_t *)data.data());
    try {
        if (_maskedFrame) {
            _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onMaskingKey(std::move(data));
            });
        } else {
            _stream->readBytes(_frameLength, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onFrameData(std::move(data));
            });
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketProtocol13::onMaskingKey(ByteArray data) {
    std::copy(data.begin(), data.end(), _frameMask.begin());
    try {
        _stream->readBytes(_frameLength, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onMaskedFrameData(std::move(data));
        });
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketProtocol13::onFrameData(ByteArray data) {
    Byte opcode;
    if (_frameOpcodeIsControl) {
        if (!_finalFrame) {
            abort();
            return;
        }
        opcode = _frameOpcode;
    } else if (_frameOpcode == 0) {
        if (_fragmentedMessageBuffer.empty()) {
            abort();
            return;
        }
        _fragmentedMessageBuffer.insert(_fragmentedMessageBuffer.end(), data.begin(), data.end());
        if (_finalFrame) {
            opcode = _fragmentedMessageOpcode;
            data = std::move(_fragmentedMessageBuffer);
            _fragmentedMessageBuffer.clear();
        }
    } else {
        if (!_fragmentedMessageBuffer.empty()) {
            abort();
            return;
        }
        if (_finalFrame) {
            opcode = _frameOpcode;
        } else {
            _fragmentedMessageOpcode = _frameOpcode;
            _fragmentedMessageBuffer = std::move(data);
        }
    }
    if (_finalFrame) {
        handleMessage(opcode, std::move(data));
    }
    if (!_clientTerminated) {
        receiveFrame();
    }
}

void WebSocketProtocol13::handleMessage(Byte opcode, ByteArray data) {
    if (_clientTerminated) {
        return;
    }
    if (opcode == 0x01) {
        // string
        WEBSOCKET_ASYNC() {
            _handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END
    } else if (opcode == 0x02) {
        // binary
        WEBSOCKET_ASYNC() {
            _handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END
    } else if (opcode == 0x08) {
        // close
        _clientTerminated = true;
        close();
    } else if (opcode == 0x09) {
        // ping
        writeFrame(true, 0x0A, data.data(), data.size());
    } else if (opcode == 0x0A) {
        // pong
        WEBSOCKET_ASYNC() {
            _handler->onPong(std::move(data));
        } WEBSOCKET_ASYNC_END
    } else {
        abort();
    }
}


WebSocketClientConnection::WebSocketClientConnection(IOLoop *ioloop, std::shared_ptr<HTTPRequest> request,
                                                     ConnectCallbackType callback)
        : _HTTPConnection(ioloop, nullptr, std::move(request), nullptr, 104857600)
        , _connectCallback(StackContext::wrap<std::shared_ptr<WebSocketClientConnection>>(std::move(callback))) {
#ifndef NDEBUG
    sWatcher->inc(SYS_WEBSOCKETCLIENTCONNECTION_COUNT);
#endif
}

WebSocketClientConnection::~WebSocketClientConnection() {
#ifndef NDEBUG
    sWatcher->dec(SYS_WEBSOCKETCLIENTCONNECTION_COUNT);
#endif
}

void WebSocketClientConnection::close() {
    if (_protocol) {
        _protocol->close();
    }
}

void WebSocketClientConnection::writeMessage(const Byte *message, size_t length, bool binary) {
    _protocol->writeMessage(message, length, binary);
}

void WebSocketClientConnection::readMessage(ReadCallbackType callback) {
    ASSERT(!_readCallback);
    callback = StackContext::wrap<boost::optional<ByteArray>>(std::move(callback));
    if (!_readQueue.empty()) {
        boost::optional<ByteArray> message = std::move(_readQueue[0]);
        _readQueue.pop_front();
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        _ioloop->spawnCallback([callback = std::move(callback), message=std::move(message)]() {
            callback(std::move(message));
        });
#else
        _ioloop->spawnCallback(std::bind([](ReadCallbackType &callback, boost::optional<ByteArray> &message) {
            callback(std::move(message));
        }, std::move(callback), std::move(message)));
#endif
    } else {
        _readCallback = std::move(callback);
    }
}

void WebSocketClientConnection::prepare() {
    _callback = [this, self=shared_from_this()](HTTPResponse response) {
        onHTTPResponse(std::move(response));
    };
    std::string key(16, {});
    Random::randBytes((Byte *)key.data(), key.size());
    _key = Base64::b64encode(key);
    const std::string &url = _request->getURL();
    std::string scheme, sep, rest;
    std::tie(scheme, sep, rest) = String::partition(url, ":");
    if (scheme == "ws") {
        scheme = "http";
    } else if (scheme == "wss") {
        scheme = "https";
    } else {
        ThrowException(ValueError, String::format("Unsupported url scheme: %s", url.c_str()));
    }
    _request->setURL(scheme + sep + rest);
    std::initializer_list<HTTPHeaders::NameValueType> headers = {
            {"Upgrade", "websocket"},
            {"Connection", "Upgrade"},
            {"Sec-WebSocket-Key", _key},
            {"Sec-WebSocket-Version", "13"}
    };
    _request->headers().update(headers);
}

void WebSocketClientConnection::handle1xx(int code) {
    _callback = nullptr;
    ASSERT(code == 101);
    ASSERT(boost::to_lower_copy(_headers->at("Upgrade")) == "websocket");
    ASSERT(boost::to_lower_copy(_headers->at("Connection")) == "upgrade");
    std::string accept = WebSocketProtocol13::computeAcceptValue(_key);
    ASSERT(_headers->at("Sec-Websocket-Accept") == accept);
    _protocol = make_unique<WebSocketClientProtocol>(this, true);
    _protocol->receiveFrame();
    if (!_timeout.expired()) {
        _ioloop->removeTimeout(_timeout);
        _timeout.reset();
    }
    ConnectCallbackType callback(std::move(_connectCallback));
    _connectCallback = nullptr;
    auto self = getSelf<WebSocketClientConnection>();
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    _ioloop->spawnCallback([callback = std::move(callback), self]() {
        callback(std::move(self));
    });
#else
    _ioloop->spawnCallback(std::bind([self](ConnectCallbackType &callback) {
        callback(std::move(self));
    }, std::move(callback)));
#endif

}

void WebSocketClientConnection::onClose() {
    onMessage(boost::none);
    _HTTPConnection::onClose();
    _callback = nullptr;
}

void WebSocketClientConnection::onHTTPResponse(HTTPResponse response) {
    if (_connectCallback) {
        ConnectCallbackType callback(std::move(_connectCallback));
        _connectCallback = nullptr;
        if (response.getError()) {
            _error = response.getError();
        } else {
            _error = MakeExceptionPtr(WebSocketError, "Non-websocket response");
        }
        auto self = getSelf<WebSocketClientConnection>();
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        _ioloop->spawnCallback([callback = std::move(callback), self]() {
            callback(std::move(self));
        });
#else
        _ioloop->spawnCallback(std::bind([self](ConnectCallbackType &callback) {
                callback(std::move(self));
            }, std::move(callback)));
#endif
    }
}

void WebSocketClientConnection::onMessage(boost::optional<ByteArray> message) {
    if (_readCallback) {
        ReadCallbackType callback = std::move(_readCallback);
        _readCallback = nullptr;
#if !defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
        _ioloop->spawnCallback([callback = std::move(callback), message=std::move(message)]() {
            callback(std::move(message));
        });
#else
        _ioloop->spawnCallback(std::bind([](ReadCallbackType &callback, boost::optional<ByteArray> &message) {
            callback(std::move(message));
        }, std::move(callback), std::move(message)));
#endif
    } else {
        _readQueue.emplace_back(std::move(message));
    }
}


void WebSocketConnect(const std::string &url, WebSocketClientConnection::ConnectCallbackType callback,
                      float connectTimeout, IOLoop *ioloop) {
    if (!ioloop) {
        ioloop = IOLoop::current();
    }
    auto request = HTTPRequest::create(url, opts::_connectTimeout=connectTimeout);
    auto connection = std::make_shared<WebSocketClientConnection>(ioloop, std::move(request), std::move(callback));
    connection->start();
}


#define WEBSOCKET_ASYNC_END2 catch (std::exception &e) { \
    LOG_ERROR(gAppLog, "Uncaught exception %s in %s", e.what(), _request->getURL().c_str()); \
    abort(); \
} catch(...) { \
    LOG_ERROR(gAppLog, "Unknown exception in %s", _request->getURL().c_str()); \
    abort(); \
}


void WebSocketClientProtocol::close() {
    if (!_serverTerminated) {
        if (!_stream->closed()) {
            writeFrame(true, 0x08, nullptr, 0);
        }
        _serverTerminated = true;
    }
    if (_clientTerminated) {
        if (!_waiting.expired()) {
            _stream->ioloop()->removeTimeout(_waiting);
            _waiting.reset();
        }
        _stream->close();
    } else if (_waiting.expired()) {
        _waiting = _stream->ioloop()->addTimeout(5.0f, [this, handler=_handler->shared_from_this()]() {
            abort();
        });
    }
}

void WebSocketClientProtocol::writeFrame(bool fin, Byte opcode, const Byte *data, size_t length) {
    auto finbit = (Byte)(fin ? 0x80 : 0x00);
    ByteArray frame;
    frame.push_back(finbit | opcode);
    auto maskBit = (Byte)(_maskOutgoing ? 0x80 : 0x00);
    if (length < 126) {
        frame.push_back((Byte)length | maskBit);
    } else if (length <= 0xFFFF) {
        frame.push_back((Byte)126 | maskBit);
        uint16_t frameLength = (uint16_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    } else {
        frame.push_back((Byte)127 | maskBit);
        uint64_t frameLength = (uint64_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    }
    if (length) {
        if (_maskOutgoing) {
            std::array<Byte, 4> mask;
            Random::randBytes(mask);
            frame.insert(frame.end(), mask.begin(), mask.end());
            frame.insert(frame.end(), data, data + length);
            applyMask(mask, frame.data() + frame.size() - length, length);
        } else {
            frame.insert(frame.end(), data, data + length);
        }
    }
    _stream->write(frame.data(), frame.size());
}

void WebSocketClientProtocol::onFrameStart(ByteArray data) {
    Byte header = data[0], payloadLen = data[1];
    _finalFrame = (header & 0x80) != 0;
    Byte reservedBits = (Byte)(header & 0x70);
    _frameOpcode = (Byte)(header & 0x0f);
    _frameOpcodeIsControl = (_frameOpcode & 0x8) != 0;
    if (reservedBits != 0) {
        abort();
        return;
    }
    _maskedFrame = (payloadLen & 0x80) != 0;
    payloadLen &= 0x7f;
    if (_frameOpcodeIsControl && payloadLen >= 126) {
        abort();
        return;
    }
    try {
        auto wrapper = std::make_shared<ReadCallbackWrapper>(this);
        if (payloadLen < 126) {
            _frameLength = payloadLen;
            if (_maskedFrame) {
                _stream->readBytes(4, std::bind(&ReadCallbackWrapper::onMaskingKey, std::move(wrapper),
                                                std::placeholders::_1));
            } else {
                _stream->readBytes(_frameLength, std::bind(&ReadCallbackWrapper::onFrameData, std::move(wrapper),
                                                           std::placeholders::_1));
            }
        } else if (payloadLen == 126) {
            _stream->readBytes(2, std::bind(&ReadCallbackWrapper::onFrameLength16, std::move(wrapper),
                                            std::placeholders::_1));
        } else if (payloadLen == 127) {
            _stream->readBytes(8, std::bind(&ReadCallbackWrapper::onFrameLength64, std::move(wrapper),
                                            std::placeholders::_1));
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketClientProtocol::onFrameLength16(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(unsigned short *)data.data());
    try {
        auto wrapper = std::make_shared<ReadCallbackWrapper>(this);
        if (_maskedFrame) {
            _stream->readBytes(4, std::bind(&ReadCallbackWrapper::onMaskingKey, std::move(wrapper),
                                            std::placeholders::_1));
        } else {
            _stream->readBytes(_frameLength, std::bind(&ReadCallbackWrapper::onFrameData, std::move(wrapper),
                                                       std::placeholders::_1));
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketClientProtocol::onFrameLength64(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(uint64_t *)data.data());
    try {
        auto wrapper = std::make_shared<ReadCallbackWrapper>(this);
        if (_maskedFrame) {
            _stream->readBytes(4, std::bind(&ReadCallbackWrapper::onMaskingKey, std::move(wrapper),
                                            std::placeholders::_1));
        } else {
            _stream->readBytes(_frameLength, std::bind(&ReadCallbackWrapper::onFrameData, std::move(wrapper),
                                                       std::placeholders::_1));
        }
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketClientProtocol::onMaskingKey(ByteArray data) {
    std::copy(data.begin(), data.end(), _frameMask.begin());
    try {
        auto wrapper = std::make_shared<ReadCallbackWrapper>(this);
        _stream->readBytes(_frameLength, std::bind(&ReadCallbackWrapper::onMaskedFrameData, std::move(wrapper),
                                                   std::placeholders::_1));
    } catch (StreamClosedError &e) {
        abort();
    }
}

void WebSocketClientProtocol::onFrameData(ByteArray data) {
    Byte opcode;
    if (_frameOpcodeIsControl) {
        if (!_finalFrame) {
            abort();
            return;
        }
        opcode = _frameOpcode;
    } else if (_frameOpcode == 0) {
        if (_fragmentedMessageBuffer.empty()) {
            abort();
            return;
        }
        _fragmentedMessageBuffer.insert(_fragmentedMessageBuffer.end(), data.begin(), data.end());
        if (_finalFrame) {
            opcode = _fragmentedMessageOpcode;
            data = std::move(_fragmentedMessageBuffer);
            _fragmentedMessageBuffer.clear();
        }
    } else {
        if (!_fragmentedMessageBuffer.empty()) {
            abort();
            return;
        }
        if (_finalFrame) {
            opcode = _frameOpcode;
        } else {
            _fragmentedMessageOpcode = _frameOpcode;
            _fragmentedMessageBuffer = std::move(data);
        }
    }
    if (_finalFrame) {
        handleMessage(opcode, std::move(data));
    }
    if (!_clientTerminated) {
        receiveFrame();
    }
}

void WebSocketClientProtocol::handleMessage(Byte opcode, ByteArray data) {
    if (_clientTerminated) {
        return;
    }
    if (opcode == 0x01) {
        // string
        WEBSOCKET_ASYNC() {
            _handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END2
    } else if (opcode == 0x02) {
        // binary
        WEBSOCKET_ASYNC() {
            _handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END2
    } else if (opcode == 0x08) {
        // close
        _clientTerminated = true;
        close();
    } else if (opcode == 0x09) {
        // ping
        writeFrame(true, 0x0A, data.data(), data.size());
    } else if (opcode == 0x0A) {
        // pong
        WEBSOCKET_ASYNC() {
            _handler->onPong(std::move(data));
        } WEBSOCKET_ASYNC_END2
    } else {
        abort();
    }
}