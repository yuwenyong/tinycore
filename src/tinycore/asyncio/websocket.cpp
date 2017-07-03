//
// Created by yuwenyong on 17-5-5.
//

#include "tinycore/asyncio/websocket.h"
#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>
#include "tinycore/crypto/base64.h"
#include "tinycore/crypto/hashlib.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"
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

void WebSocketHandler::writeMessage(const Byte *message, size_t length) {
    _wsConnection->writeMessage(message, length, true);
}

void WebSocketHandler::writeMessage(const char *message) {
    _wsConnection->writeMessage((const Byte *)message, strlen(message), false);
}

void WebSocketHandler::writeMessage(const std::string &message) {
    _wsConnection->writeMessage((const Byte *)message.data(), message.size(), false);
}

void WebSocketHandler::writeMessage(const ByteArray &message) {
    _wsConnection->writeMessage(message.data(), message.size(), true);
}

void WebSocketHandler::onOpen(const StringVector &args) {

}

void WebSocketHandler::onClose() {

}

void WebSocketHandler::close() {
    _wsConnection->close();
}

void WebSocketHandler::onConnectionClose() {
    if (_wsConnection) {
        setClientTerminated(true);
        onClose();
    }
}

void WebSocketHandler::setClientTerminated(bool clientTerminated) {
    _wsConnection->setClientTerminated(clientTerminated);
}

bool WebSocketHandler::getClientTerminated() const {
    return _wsConnection->getClientTerminated();
}

void WebSocketHandler::execute(TransformsType &transforms, StringVector args) {
    _openArgs = std::move(args);
    std::string webSocketVersion = _request->getHTTPHeaders()->get("Sec-WebSocket-Version");
    if (webSocketVersion == "8" || webSocketVersion == "7") {
        _wsConnection = make_unique<WebSocketProtocol8>(getSelf<WebSocketHandler>());
        _wsConnection->acceptConnection();
    } else if (!webSocketVersion.empty()) {
        auto stream = fetchStream();
        const char *error = "HTTP/1.1 426 Upgrade Required\r\n"
                "Sec-WebSocket-Version: 8\r\n\r\n";
        stream->write((const Byte *)error, strlen(error));
        stream->close();
    } else {
        _wsConnection = make_unique<WebSocketProtocol76>(getSelf<WebSocketHandler>());
        _wsConnection->acceptConnection();
    }
}

#define WEBSOCKET_ASYNC() try
#define WEBSOCKET_ASYNC_END catch (std::exception &e) { \
    Log::error("Uncaught exception %s in %s", e.what(), _request->getPath().c_str()); \
    abort(); \
}

void WebSocketProtocol76::acceptConnection() {
    try {
        handleWebSocketHeaders();
    } catch (ValueError &e) {
        Log::debug("Malformed WebSocket request received");
        abort();
        return;
    }
    std::string scheme, origin;
    scheme = _request->getProtocol() == "https" ? "wss" : "ws";
    origin = _request->getHTTPHeaders()->at("Origin");
    auto stream = fetchStream();
    ASSERT(stream);
    std::string initial = String::format("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Server: %s\r\n"
                                                 "Sec-WebSocket-Origin: %s\r\n"
                                                 "Sec-WebSocket-Location: %s://%s%s\r\n\r\n",
                                         TINYCORE_VER, origin.c_str(), scheme.c_str(), _request->getHost().c_str(),
                                         _request->getURI().c_str());
    stream->write((const Byte *)initial.data(), initial.size());
    stream->readBytes(8, std::bind(&WebSocketProtocol76::handleChallenge, this, fetchHandler(), std::placeholders::_1));
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

void WebSocketProtocol76::close() {
    auto stream = fetchStream();
    if (_clientTerminated && !_waiting.expired()) {
        sIOLoop->removeTimeout(_waiting);
        _waiting.reset();
        stream->close();
    } else {
        const char data[] = {'\xff', '\x00'};
        stream->write((const Byte *)data, sizeof(data));
        _waiting = sIOLoop->addTimeout(5.0f, std::bind(&WebSocketProtocol76::onAbort, this, fetchHandler()));
    }
}

void WebSocketProtocol76::writeMessage(const Byte *message, size_t length, bool binary) {
    ByteArray buffer;
    buffer.push_back('\x00');
    buffer.insert(buffer.end(), message, message + length);
    buffer.push_back('\xff');
    auto stream = fetchStream();
    stream->write(buffer.data(), buffer.size());
}

void WebSocketProtocol76::handleChallenge(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    std::string challengeResp;
    try {
        std::string challenge((const char *)data.data(), data.size());
        challengeResp = challengeResponse(challenge);
    } catch (ValueError &e) {
        Log::debug("Malformed key data in WebSocket request");
        abort();
        return;
    }
    writeResponse(std::move(handler), challengeResp);
}

void WebSocketProtocol76::writeResponse(std::shared_ptr<WebSocketHandler> handler, const std::string &challenge) {
    auto stream = fetchStream();
    ASSERT(stream);
    stream->write((const Byte *)challenge.data(), challenge.size());
    WEBSOCKET_ASYNC() {
        handler->open();
    } WEBSOCKET_ASYNC_END
    receiveMessage(std::move(handler));
}

void WebSocketProtocol76::handleWebSocketHeaders() const {
    auto headers = _request->getHTTPHeaders();
    const StringVector fields = {"Origin", "Host", "Sec-Websocket-Key1", "Sec-Websocket-Key2"};
    if (boost::to_lower_copy(headers->get("Upgrade")) != "websocket" ||
        boost::to_lower_copy(headers->get("Connection")) != "upgrade") {
        ThrowException(ValueError, "Missing/Invalid WebSocket headers");
    }
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

void WebSocketProtocol76::receiveMessage(std::shared_ptr<WebSocketHandler> handler) {
    auto stream = fetchStream();
    ASSERT(stream);
    stream->readBytes(1, std::bind(&WebSocketProtocol76::onFrameType, this, std::move(handler), std::placeholders::_1));
}

void WebSocketProtocol76::onFrameType(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    auto stream = fetchStream();
    ASSERT(stream);
    unsigned char frameType = data[0];
    if (frameType == 0x00) {
        std::string delimiter('\xff', 1);;
        stream->readUntil(std::move(delimiter), std::bind(&WebSocketProtocol76::onEndDelimiter, this,
                                                          std::move(handler), std::placeholders::_1));
    } else if (frameType == 0xff) {
        stream->readBytes(1, std::bind(&WebSocketProtocol76::onLengthIndicator, this, std::move(handler),
                                       std::placeholders::_1));
    } else {
        abort();
    }
}

void WebSocketProtocol76::onEndDelimiter(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    if (!_clientTerminated) {
        WEBSOCKET_ASYNC() {
            data.pop_back();
            handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END;
    }
    if (!_clientTerminated) {
        receiveMessage(std::move(handler));
    }
}

void WebSocketProtocol76::onLengthIndicator(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    unsigned char byte = data[0];
    if (byte != 0x00) {
        abort();
        return;
    }
    _clientTerminated = true;
    close();
}

void WebSocketProtocol76::onAbort(std::shared_ptr<WebSocketHandler> handler) {
    abort();
}


void WebSocketProtocol8::acceptConnection() {
    try {
        handleWebSocketHeaders();
        onAcceptConnection();
    } catch (ValueError &e) {
        Log::debug("Malformed WebSocket request received");
        abort();
        return;
    }
}

void WebSocketProtocol8::writeMessage(const Byte *message, size_t length, bool binary) {
    Byte opcode = (Byte)(binary ? 0x02 : 0x01);
    writeFrame(true, opcode, message, length);
}

void WebSocketProtocol8::close() {
    writeFrame(true, 0x08, nullptr, false);
    _startedClosingHandshake = true;
    _waiting = sIOLoop->addTimeout(5.0f, std::bind(&WebSocketProtocol8::onAbort, this, fetchHandler()));
}

void WebSocketProtocol8::handleWebSocketHeaders() const {
    auto headers = _request->getHTTPHeaders();
    const StringVector fields = {"Origin", "Host", "Sec-Websocket-Key", "Sec-Websocket-Version"};
    StringVector connection = String::split(headers->get("Connection"), ',');
    for (auto &v: connection) {
        boost::trim(v);
        boost::to_lower(v);
    }
    if (_request->getMethod() != "GET" || boost::to_lower_copy(headers->get("Upgrade")) != "websocket" ||
        std::find(connection.begin(), connection.end(), "upgrade") == connection.end()) {
        ThrowException(ValueError, "Missing/Invalid WebSocket headers");
    }
    for (auto &field: fields) {
        if (headers->get(field).empty()) {
            ThrowException(ValueError, "Missing/Invalid WebSocket headers");
        }
    }
}

std::string WebSocketProtocol8::challengeResponse() const {
    SHA1Object sha1;
    sha1.update(_request->getHTTPHeaders()->get("Sec-Websocket-Key"));
    sha1.update("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return Base64::b64encode(sha1.digest());
}

void WebSocketProtocol8::onAcceptConnection() {
    std::string challengeResp = challengeResponse();
    auto stream = fetchStream();
    ASSERT(stream);
    std::string initial = String::format("HTTP/1.1 101 Switching Protocols\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Sec-WebSocket-Accept: %s\r\n\r\n", challengeResp.c_str());
    stream->write((const Byte *)initial.data(), initial.size());
    auto handler = fetchHandler();
    WEBSOCKET_ASYNC() {
        handler->open();
    } WEBSOCKET_ASYNC_END
    receiveFrame(std::move(handler));
}

void WebSocketProtocol8::writeFrame(bool fin, Byte opcode, const Byte *data, size_t length) {
    Byte finbit = (Byte)(fin ? 0x80 : 0x00);
    ByteArray frame;
    frame.push_back(finbit | opcode);
    if (length < 126) {
        frame.push_back((Byte)length);
    } else if (length <= 0xFFFF) {
        frame.push_back(126);
        uint16_t frameLength = (uint16_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    } else {
        frame.push_back(127);
        uint64_t frameLength = (uint64_t)length;
        boost::endian::native_to_big_inplace(frameLength);
        frame.insert(frame.end(), (Byte *)&frameLength, (Byte *)&frameLength + sizeof(frameLength));
    }
    if (length) {
        frame.insert(frame.end(), data, data + length);
    }
    auto stream = fetchStream();
    stream->write(frame.data(), frame.size());
}

void WebSocketProtocol8::receiveFrame(std::shared_ptr<WebSocketHandler> handler) {
    auto stream = fetchStream();
    stream->readBytes(2, std::bind(&WebSocketProtocol8::onFrameStart, this, std::move(handler), std::placeholders::_1));
}

void WebSocketProtocol8::onFrameStart(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    Byte header = data[0], payloadLen = data[1];
    _finalFrame = (header & 0x80) != 0;
    _frameOpcode = (Byte)(header & 0x0f);
    if ((payloadLen & 0x80) == 0) {
        abort();
    }
    auto stream = fetchStream();
    payloadLen &= 0x7f;
    if (payloadLen < 126) {
        _frameLength = payloadLen;
        stream->readBytes(4, std::bind(&WebSocketProtocol8::onMaskKey, this, std::move(handler),
                                       std::placeholders::_1));
    } else if (payloadLen == 126) {
        stream->readBytes(2, std::bind(&WebSocketProtocol8::onFrameLength16, this, std::move(handler),
                                       std::placeholders::_1));
    } else if (payloadLen == 127) {
        stream->readBytes(8, std::bind(&WebSocketProtocol8::onFrameLength64, this, std::move(handler),
                                       std::placeholders::_1));
    }
}

void WebSocketProtocol8::onFrameLength16(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(unsigned short *)data.data());
    auto stream = fetchStream();
    stream->readBytes(4, std::bind(&WebSocketProtocol8::onMaskKey, this, std::move(handler),
                                   std::placeholders::_1));
}

void WebSocketProtocol8::onFrameLength64(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(uint64_t *)data.data());
    auto stream = fetchStream();
    stream->readBytes(4, std::bind(&WebSocketProtocol8::onMaskKey, this, std::move(handler),
                                   std::placeholders::_1));
}

void WebSocketProtocol8::onMaskKey(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    std::copy(data.begin(), data.end(), _frameMask.begin());
    auto stream = fetchStream();
    stream->readBytes(_frameLength, std::bind(&WebSocketProtocol8::onFrameData, this, std::move(handler),
                                              std::placeholders::_1));
}

void WebSocketProtocol8::onFrameData(std::shared_ptr<WebSocketHandler> handler, ByteArray data) {
    for (size_t i = 0; i != data.size(); ++i) {
        data[i] ^= _frameMask[i % 4];
    }
    if (!_finalFrame) {
        if (!_fragmentedMessageBuffer.empty()) {
            _fragmentedMessageBuffer.insert(_fragmentedMessageBuffer.end(), data.begin(), data.end());
        } else {
            _fragmentedMessageOpcode = _frameOpcode;
            _fragmentedMessageBuffer = std::move(data);
        }
    } else {
        Byte opcode;
        if (_frameOpcode == 0x00) {
            _fragmentedMessageBuffer.insert(_fragmentedMessageBuffer.end(), data.begin(), data.end());
            opcode = _fragmentedMessageOpcode;
            data = std::move(_fragmentedMessageBuffer);
        } else {
            opcode = _frameOpcode;
        }
        handleMessage(opcode, std::move(data));
    }
    if (!_clientTerminated) {
        receiveFrame(std::move(handler));
    }
}

void WebSocketProtocol8::handleMessage(Byte opcode, ByteArray data) {
    if (_clientTerminated) {
        return;
    }
    auto handler = fetchHandler();
    if (opcode == 0x01) {
        // string
        WEBSOCKET_ASYNC() {
            handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END
    } else if (opcode == 0x02) {
        // binary
        WEBSOCKET_ASYNC() {
            handler->onMessage(std::move(data));
        } WEBSOCKET_ASYNC_END
    } else if (opcode == 0x08) {
        // close
        _clientTerminated = true;
        if (!_startedClosingHandshake) {
            writeFrame(true, 0x08, nullptr, 0);
        }
        auto stream = fetchStream();
        stream->close();
    } else if (opcode == 0x09) {
        // ping
        writeFrame(true, 0x0A, data.data(), data.size());
    } else if (opcode == 0x0A) {
        // pong
    } else {
        abort();
    }
}

void WebSocketProtocol8::onAbort(std::shared_ptr<WebSocketHandler> handler) {
    abort();
}