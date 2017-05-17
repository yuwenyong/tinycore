//
// Created by yuwenyong on 17-5-5.
//

#ifndef TINYCORE_WEBSOCKET_H
#define TINYCORE_WEBSOCKET_H

#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"
#include "tinycore/networking/web.h"
#include "tinycore/networking/ioloop.h"


#define _NOT_SUPPORTED(func) void func() { ThrowException(Exception, #func " not supported for Web Sockets"); }


class WebSocketRequest;
typedef std::unique_ptr<WebSocketRequest> WebSocketRequestPtr;


class TC_COMMON_API WebSocketHandler: public RequestHandler {
public:
    typedef HTTPServerRequest::BufferType BufferType;

    WebSocketHandler(Application *application, HTTPServerRequestPtr request);
    ~WebSocketHandler();
    void writeMessage(const char *message, size_t length);

    void writeMessage(const char *message) {
        writeMessage(message, strlen(message));
    }

    void writeMessage(const std::string &message) {
        writeMessage(message.c_str(), message.length());
    }

    void writeMessage(const std::vector<char> &message) {
        writeMessage(message.data(), message.size());
    }

    void writeMessage(const JSONType &message) {
        std::ostringstream messageBuffer;
        boost::property_tree::write_json(messageBuffer, message);
        std::string content = messageBuffer.str();
        writeMessage(content);
    }

    virtual void onOpen(const StringVector &args);
    virtual void onMessage(const char *data, size_t length) = 0;
    virtual void onClose();
    void close();
    void onConnectionClose() override;
protected:
    void execute(TransformsType &transforms, StringVector args) override;
    void handleChallenge(BufferType &data);
    void writeResponse(const std::string &challenge);
    void abort();
    void receiveMessage();
    void onFrameType(BufferType &data);
    void onEndDelimiter(BufferType &data);
    void onLengthIndicator(BufferType &data);

    BaseIOStreamPtr getStream() {
        return _stream.lock();
    }

    BaseIOStreamWPtr _stream;
    BaseIOStreamPtr _streamKeeper;
    bool _clientTerminated;
    StringVector _openArgs;
    WebSocketRequestPtr _wsRequest;
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
    WebSocketRequest(HTTPServerRequestPtr request)
            : _request(std::move(request)) {

    }

    std::string challengeResponse(const std::string &challenge);
    void handleWebSocketHeaders() const;
protected:
    std::string calculatePart(const std::string &key);
    std::string generateChallengeResponse(const std::string &part1, const std::string &part2, const std::string &part3);

    HTTPServerRequestPtr _request;
};

#endif //TINYCORE_WEBSOCKET_H
