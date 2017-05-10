//
// Created by yuwenyong on 17-5-5.
//

#include "tinycore/networking/websocket.h"
#include <boost/algorithm/string.hpp>
#include <boost/endian/conversion.hpp>
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"
#include "tinycore/crypto/hashlib.h"
#include "tinycore/utilities/string.h"


WebSocketHandler::WebSocketHandler(Application *application, HTTPRequestPtr request)
        : RequestHandler(application, request)
        , _clientTerminated(false) {
    _streamKeeper = request->getConnection()->releaseStream();
    _stream = _streamKeeper;
#ifndef NDEBUG
    sWatcher->inc(SYS_WEBSOCKETHANDLER_COUNT);
#endif
}

WebSocketHandler::~WebSocketHandler() {
#ifndef NDEBUG
    sWatcher->dec(SYS_WEBSOCKETHANDLER_COUNT);
#endif
}

void WebSocketHandler::writeMessage(const char *message, size_t length) {
    std::vector<char> buffer;
    buffer.push_back('\x00');
    buffer.insert(buffer.end(), message, message + length);
    buffer.push_back('\xff');
    auto stream = getStream();
    BufferType chunk(buffer.data(), buffer.size());
    stream->write(chunk);
}

void WebSocketHandler::onOpen(const StringVector &args) {

}

void WebSocketHandler::onClose() {

}

void WebSocketHandler::close() {
    auto stream = getStream();
    if (_clientTerminated && !_waiting.expired()) {
        sIOLoop->removeTimeout(_waiting);
        _waiting.reset();
        stream->close();
    } else {
        char data[] = {'\xff', '\x00'};
        BufferType chunk(data, sizeof(data));
        stream->write(chunk);
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
    origin = _request->getHTTPHeader()->getItem("Origin");
    std::string initial = String::format("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
                                                 "Upgrade: WebSocket\r\n"
                                                 "Connection: Upgrade\r\n"
                                                 "Server: %s\r\n"
                                                 "Sec-WebSocket-Origin: %s\r\n"
                                                 "Sec-WebSocket-Location: %s://%s%s\r\n\r\n",
                                         TINYCORE_VER, origin.c_str(), scheme.c_str(), _request->getHost().c_str(),
                                         _request->getURI().c_str());
    auto stream = std::move(_streamKeeper);
    ASSERT(stream);
    BufferType buffer(initial.c_str(), initial.size());
    stream->write(buffer);
    stream->readBytes(8, std::bind(&WebSocketHandler::handleChallenge, getSelf<WebSocketHandler>(),
                                   std::placeholders::_1));
}

void WebSocketHandler::handleChallenge(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    std::string challengeResponse;
    try {
        std::string challenge(boost::asio::buffer_cast<const char *>(data), boost::asio::buffer_size(data));
        challengeResponse = _wsRequest->challengeResponse(challenge);
    } catch (ValueError &e) {
        Log::debug("Malformed key data in WebSocket request");
        abort();
        return;
    }
    writeResponse(challengeResponse);
}

void WebSocketHandler::writeResponse(const std::string &challenge) {
    auto stream = getStream();
    ASSERT(stream);
    BufferType buffer(challenge.c_str(), challenge.size());
    stream->write(buffer);
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
    auto stream = getStream();
    stream->close();
}

void WebSocketHandler::receiveMessage() {
    auto stream = getStream();
    _streamKeeper.reset();
    stream->readBytes(1, std::bind(&WebSocketHandler::onFrameType, getSelf<WebSocketHandler>(), std::placeholders::_1));
}

void WebSocketHandler::onFrameType(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    char frameType = boost::asio::buffer_cast<const char *>(data)[0];
    if (frameType == 0x00) {
        std::string delimiter('\xff', 1);
        _streamKeeper.reset();
        stream->readUntil(std::move(delimiter), std::bind(&WebSocketHandler::onEndDelimiter,
                                                          getSelf<WebSocketHandler>(),
                                                          std::placeholders::_1));
    } else if (frameType == 0xff) {
        _streamKeeper.reset();
        stream->readBytes(1, std::bind(&WebSocketHandler::onLengthIndicator, getSelf<WebSocketHandler>(),
                                       std::placeholders::_1));
    } else {
        abort();
    }
}

void WebSocketHandler::onEndDelimiter(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    if (!_clientTerminated) {
        try {
            const char *frame = boost::asio::buffer_cast<const char *>(data);
            size_t frameSize = boost::asio::buffer_size(data);
            if (frameSize > 0) {
                --frameSize;
            }
            onMessage(frame, frameSize);
        } catch (std::exception &e) {
            Log::error("Uncaught exception %s in %s", e.what(), _request->getPath().c_str());
            abort();
        }
    }
    if (!_clientTerminated) {
        receiveMessage();
    }
}

void WebSocketHandler::onLengthIndicator(BufferType &data) {
    auto stream = getStream();
    if (stream->dying()) {
        _streamKeeper = stream;
    }
    char byte = boost::asio::buffer_cast<const char *>(data)[0];
    if (byte != 0x00) {
        abort();
        return;
    }
    _clientTerminated = true;
    close();
}


std::string WebSocketRequest::challengeResponse(const std::string &challenge) {
    auto headers = _request->getHTTPHeader();
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
    auto headers = _request->getHTTPHeader();
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
    int spaces = String::count(key, boost::is_space());
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
    m.update(part1.data(), part1.length());
    m.update(part2.data(), part2.length());
    m.update(part3.data(), part3.length());
    return m.hex();
}