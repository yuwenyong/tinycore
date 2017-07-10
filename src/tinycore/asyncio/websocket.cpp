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

boost::optional<std::string> WebSocketHandler::selectSubProtocol(const StringVector &subProtocols) const {
    return boost::none;
}

void WebSocketHandler::onOpen(const StringVector &args) {

}

void WebSocketHandler::onClose() {

}

void WebSocketHandler::close() {
    _wsConnection->close();
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

void WebSocketHandler::setClientTerminated(bool clientTerminated) {
    _wsConnection->setClientTerminated(clientTerminated);
}

bool WebSocketHandler::getClientTerminated() const {
    return _wsConnection->getClientTerminated();
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
        Log::debug("Malformed key data in WebSocket request");
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
        } WEBSOCKET_ASYNC_END;
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
        doAcceptConnection();
    } catch (ValueError &e) {
        Log::debug("Malformed WebSocket request received");
        abort();
        return;
    }
}

void WebSocketProtocol13::writeMessage(const Byte *message, size_t length, bool binary) {
    Byte opcode = (Byte)(binary ? 0x02 : 0x01);
    writeFrame(true, opcode, message, length);
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

void WebSocketProtocol13::handleWebSocketHeaders() const {
    auto headers = _request->getHTTPHeaders();
    const StringVector fields = {"Host", "Sec-Websocket-Key", "Sec-Websocket-Version"};
    for (auto &field: fields) {
        if (headers->get(field).empty()) {
            ThrowException(ValueError, "Missing/Invalid WebSocket headers");
        }
    }
}

std::string WebSocketProtocol13::challengeResponse() const {
    SHA1Object sha1;
    sha1.update(_request->getHTTPHeaders()->get("Sec-Websocket-Key"));
    sha1.update("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return Base64::b64encode(sha1.digest());
}

void WebSocketProtocol13::doAcceptConnection() {
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
    if ((payloadLen & 0x80) == 0) {
        abort();
        return;
    }
    payloadLen &= 0x7f;
    if (payloadLen < 126) {
        _frameLength = payloadLen;
        _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onMaskKey(std::move(data));
        });
    } else if (payloadLen == 126) {
        _stream->readBytes(2, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onFrameLength16(std::move(data));
        });
    } else if (payloadLen == 127) {
        _stream->readBytes(8, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onFrameLength64(std::move(data));
        });
    }
}

void WebSocketProtocol13::onFrameLength16(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(unsigned short *)data.data());
    _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
        onMaskKey(std::move(data));
    });
}

void WebSocketProtocol13::onFrameLength64(ByteArray data) {
    _frameLength = boost::endian::big_to_native(*(uint64_t *)data.data());
    _stream->readBytes(4, [this, handler=_handler->shared_from_this()](ByteArray data) {
        onMaskKey(std::move(data));
    });
}

void WebSocketProtocol13::onMaskKey(ByteArray data) {
    std::copy(data.begin(), data.end(), _frameMask.begin());
    _stream->readBytes(_frameLength, [this, handler=_handler->shared_from_this()](ByteArray data) {
        onFrameData(std::move(data));
    });
}

void WebSocketProtocol13::onFrameData(ByteArray data) {
    for (size_t i = 0; i != data.size(); ++i) {
        data[i] ^= _frameMask[i % 4];
    }
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
    } else {
        abort();
    }
}
