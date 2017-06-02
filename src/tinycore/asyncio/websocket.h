//
// Created by yuwenyong on 17-5-5.
//

#ifndef TINYCORE_WEBSOCKET_H
#define TINYCORE_WEBSOCKET_H

#include "tinycore/common/common.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/web.h"
#include "tinycore/common/errors.h"


#define _NOT_SUPPORTED(func) void func() { ThrowException(Exception, #func " not supported for Web Sockets"); }


class WebSocketRequest;


class TC_COMMON_API WebSocketHandler: public RequestHandler {
public:

    WebSocketHandler(Application *application, std::shared_ptr<HTTPServerRequest> request);
    ~WebSocketHandler();
    void writeMessage(const Byte *message, size_t length);

    void writeMessage(const char *message) {
        writeMessage((const Byte *)message, strlen(message));
    }

    void writeMessage(const std::string &message) {
        writeMessage((const Byte *)message.data(), message.size());
    }

    void writeMessage(const ByteArray &message) {
        writeMessage(message.data(), message.size());
    }

    void writeMessage(const SimpleJSONType &message) {
        writeMessage(String::fromJSON(message));
    }

#ifdef HAS_RAPID_JSON
    void writeMessage(const rapidjson::Document &message) {
        writeMessage(String::fromJSON(message));
    }
#endif

    virtual void onOpen(const StringVector &args);
    virtual void onMessage(const ByteArray data) = 0;
    virtual void onClose();
    void close();
    void onConnectionClose() override;
protected:
    void execute(TransformsType &transforms, StringVector args) override;
    void handleChallenge(ByteArray data);
    void writeResponse(const std::string &challenge);
    void abort();
    void receiveMessage();
    void onFrameType(ByteArray data);
    void onEndDelimiter(ByteArray data);
    void onLengthIndicator(ByteArray data);

    std::shared_ptr<BaseIOStream> fetchStream() {
        return _streamObserver.lock();
    }

    std::weak_ptr<BaseIOStream> _streamObserver;
    std::shared_ptr<BaseIOStream> _stream;
    bool _clientTerminated;
    StringVector _openArgs;
    std::unique_ptr<WebSocketRequest> _wsRequest;
    Timeout _waiting;
public:
    _NOT_SUPPORTED(write)
    _NOT_SUPPORTED(redirect)
    _NOT_SUPPORTED(setHeader)
    _NOT_SUPPORTED(sendError)
    _NOT_SUPPORTED(setCookie)
    _NOT_SUPPORTED(setStatus)
    _NOT_SUPPORTED(flush)
    _NOT_SUPPORTED(finish)
};


class WebSocketRequest {
public:
    WebSocketRequest(std::shared_ptr<HTTPServerRequest> request)
            : _request(std::move(request)) {

    }

    std::string challengeResponse(const std::string &challenge);
    void handleWebSocketHeaders() const;
protected:
    std::string calculatePart(const std::string &key);
    std::string generateChallengeResponse(const std::string &part1, const std::string &part2, const std::string &part3);

    std::shared_ptr<HTTPServerRequest> _request;
};

#endif //TINYCORE_WEBSOCKET_H
