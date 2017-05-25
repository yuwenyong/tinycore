//
// Created by yuwenyong on 17-5-5.
//

#include "tinycore/asyncio/websocket.h"
#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>
#include "tinycore/crypto/hashlib.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"
#include "tinycore/utilities/string.h"


WebSocketHandler::WebSocketHandler(Application *application, std::shared_ptr<HTTPServerRequest> request)
        : RequestHandler(application, request)
        , _clientTerminated(false) {
    _stream = request->getConnection()->releaseStream();
    _streamObserver = _stream;
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
    ByteArray buffer;
    buffer.push_back('\x00');
    buffer.insert(buffer.end(), message, message + length);
    buffer.push_back('\xff');
    auto stream = fetchStream();
    stream->write(buffer.data(), buffer.size());
}

void WebSocketHandler::onOpen(const StringVector &args) {

}

void WebSocketHandler::onClose() {

}

void WebSocketHandler::close() {
    auto stream = fetchStream();
    if (_clientTerminated && !_waiting.expired()) {
        sIOLoop->removeTimeout(_waiting);
        _waiting.reset();
        stream->close();
    } else {
        const char data[] = {'\xff', '\x00'};
        stream->write((const Byte *)data, sizeof(data));
        _waiting = sIOLoop->addTimeout(5.0f, std::bind(&WebSocketHandler::abort, getSelf<WebSocketHandler>()));
    }
}

void WebSocketHandler::onConnectionClose() {
    _clientTerminated = true;
    onClose();
}

void WebSocketHandler::execute(TransformsType &transforms, StringVector args) {
    std::cout << "OnExecute" << std::endl;
    _openArgs = std::move(args);
    try {
        _wsRequest = make_unique<WebSocketRequest>(_request);
        _wsRequest->handleWebSocketHeaders();
    } catch (ValueError &e) {
        Log::debug("Malformed WebSocket request received");
        abort();
        return;
    }
    std::string scheme, origin;
    scheme = _request->getProtocol() == "https" ? "wss" : "ws";
    origin = _request->getHTTPHeaders()->at("Origin");
    std::string initial = String::format("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Server: %s\r\n"
                                                 "Sec-WebSocket-Origin: %s\r\n"
                                                 "Sec-WebSocket-Location: %s://%s%s\r\n\r\n",
                                         TINYCORE_VER, origin.c_str(), scheme.c_str(), _request->getHost().c_str(),
                                         _request->getURI().c_str());
    auto stream = std::move(_stream);
    ASSERT(stream);
    stream->write((const Byte *)initial.data(), initial.size());
    stream->readBytes(8, std::bind(&WebSocketHandler::handleChallenge, getSelf<WebSocketHandler>(),
                                   std::placeholders::_1, std::placeholders::_2));
}

void WebSocketHandler::handleChallenge(Byte *data, size_t length) {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    std::string challengeResponse;
    try {
        std::string challenge((const char *)data, length);
        challengeResponse = _wsRequest->challengeResponse(challenge);
    } catch (ValueError &e) {
        Log::debug("Malformed key data in WebSocket request");
        abort();
        return;
    }
    writeResponse(challengeResponse);
}

void WebSocketHandler::writeResponse(const std::string &challenge) {
    auto stream = fetchStream();
    ASSERT(stream);
    stream->write((const Byte *)challenge.data(), challenge.size());
    try {
        onOpen(_openArgs);
    } catch (std::exception &e) {
        Log::error("Uncaught exception %s in %s", e.what(), _request->getPath().c_str());
        abort();
    }
    receiveMessage();
}

void WebSocketHandler::abort() {
    _clientTerminated = true;
    auto stream = fetchStream();
    stream->close();
}

void WebSocketHandler::receiveMessage() {
    auto stream = fetchStream();
    _stream.reset();
    stream->readBytes(1, std::bind(&WebSocketHandler::onFrameType, getSelf<WebSocketHandler>(), std::placeholders::_1,
                                   std::placeholders::_2));
}

void WebSocketHandler::onFrameType(Byte *data, size_t length) {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    unsigned char frameType = data[0];
    if (frameType == 0x00) {
        std::string delimiter('\xff', 1);
        _stream.reset();
        stream->readUntil(std::move(delimiter), std::bind(&WebSocketHandler::onEndDelimiter,
                                                          getSelf<WebSocketHandler>(),
                                                          std::placeholders::_1, std::placeholders::_2));
    } else if (frameType == 0xff) {
        _stream.reset();
        stream->readBytes(1, std::bind(&WebSocketHandler::onLengthIndicator, getSelf<WebSocketHandler>(),
                                       std::placeholders::_1, std::placeholders::_2));
    } else {
        abort();
    }
}

void WebSocketHandler::onEndDelimiter(Byte *data, size_t length) {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    if (!_clientTerminated) {
        try {
            onMessage(data, length - 1);
        } catch (std::exception &e) {
            Log::error("Uncaught exception %s in %s", e.what(), _request->getPath().c_str());
            abort();
        }
    }
    if (!_clientTerminated) {
        receiveMessage();
    }
}

void WebSocketHandler::onLengthIndicator(Byte *data, size_t length) {
    auto stream = fetchStream();
    if (stream->dying()) {
        _stream = stream;
    }
    unsigned char byte = data[0];
    if (byte != 0x00) {
        abort();
        return;
    }
    _clientTerminated = true;
    close();
}


std::string WebSocketRequest::challengeResponse(const std::string &challenge) {
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

void WebSocketRequest::handleWebSocketHeaders() const {
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

std::string WebSocketRequest::calculatePart(const std::string &key) {
    std::string number = String::filter(key, boost::is_digit());
    size_t spaces = String::count(key, boost::is_space());
    if (number.empty() || spaces == 0) {
        ThrowException(ValueError, "");
    }
    unsigned int keyNumber = (unsigned int)(std::stoul(number) / spaces);
    boost::endian::native_to_big_inplace(keyNumber);
    return std::string((char *)&keyNumber, 4);
}

std::string WebSocketRequest::generateChallengeResponse(const std::string &part1, const std::string &part2,
                                                        const std::string &part3) {
    MD5Object m;
    m.update(part1);
    m.update(part2);
    m.update(part3);
    return m.hex();
}