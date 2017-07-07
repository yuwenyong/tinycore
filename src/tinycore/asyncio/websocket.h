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


class WebSocketProtocol;


class TC_COMMON_API WebSocketHandler: public RequestHandler {
public:
    friend class WebSocketProtocol;

    WebSocketHandler(Application *application, std::shared_ptr<HTTPServerRequest> request);
    ~WebSocketHandler();

    void open() {
        onOpen(_openArgs);
    }

    void writeMessage(const Byte *message, size_t length);

    void writeMessage(const char *message);

    void writeMessage(const std::string &message);

    void writeMessage(const ByteArray &message);

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
    void setClientTerminated(bool clientTerminated);
    bool getClientTerminated() const;
protected:
    void execute(TransformsType &transforms, StringVector args) override;

    std::shared_ptr<BaseIOStream> _stream;
    StringVector _openArgs;
    std::unique_ptr<WebSocketProtocol> _wsConnection;
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


class TC_COMMON_API WebSocketProtocol {
public:
    WebSocketProtocol(std::shared_ptr<WebSocketHandler> handler)
            : _handler(handler)
            , _request(handler->getRequest())
            , _stream(handler->_stream) {

    }

    virtual ~WebSocketProtocol() {}

    std::shared_ptr<WebSocketHandler> fetchHandler() const {
        return _handler.lock();
    }

    std::shared_ptr<BaseIOStream> fetchStream() const {
        return _stream.lock();
    }

    void setClientTerminated(bool clientTerminated) {
        _clientTerminated = clientTerminated;
    }

    bool getClientTerminated() const {
        return _clientTerminated;
    }

    virtual void acceptConnection() = 0;
    virtual void writeMessage(const Byte *message, size_t length, bool binary=false) = 0;
    virtual void close() = 0;
protected:
    void abort() {
        _clientTerminated = true;
        auto stream = fetchStream();
        if (stream) {
            stream->close();
        }
    }

    std::weak_ptr<WebSocketHandler> _handler;
    std::shared_ptr<HTTPServerRequest> _request;
    std::weak_ptr<BaseIOStream> _stream;
    bool _clientTerminated{false};
};


class TC_COMMON_API WebSocketProtocol76: public WebSocketProtocol {
public:
    WebSocketProtocol76(std::shared_ptr<WebSocketHandler> handler)
            : WebSocketProtocol(std::move(handler)) {

    }

    void acceptConnection() override;
    std::string challengeResponse(const std::string &challenge) const;
    void close() override;
    void writeMessage(const Byte *message, size_t length, bool binary=false) override;
protected:
    void handleChallenge(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void writeResponse(std::shared_ptr<WebSocketHandler> handler, const std::string &challenge);
    void handleWebSocketHeaders() const;
    std::string calculatePart(const std::string &key) const;
    std::string generateChallengeResponse(const std::string &part1, const std::string &part2,
                                          const std::string &part3) const;
    void receiveMessage(std::shared_ptr<WebSocketHandler> handler);
    void onFrameType(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onEndDelimiter(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onLengthIndicator(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onAbort(std::shared_ptr<WebSocketHandler> handler);

//    std::string _challenge;
    Timeout _waiting;
};


class TC_COMMON_API WebSocketProtocol8: public WebSocketProtocol {
public:
    WebSocketProtocol8(std::shared_ptr<WebSocketHandler> handler)
            : WebSocketProtocol(std::move(handler)) {

    }

    void acceptConnection() override;
    void writeMessage(const Byte *message, size_t length, bool binary=false) override;
    void close() override;
protected:
    void handleWebSocketHeaders() const;
    std::string challengeResponse() const;
    void onAcceptConnection();
    void writeFrame(bool fin, Byte opcode, const Byte *data, size_t length);
    void receiveFrame(std::shared_ptr<WebSocketHandler> handler);
    void onFrameStart(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onFrameLength16(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onFrameLength64(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onMaskKey(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void onFrameData(std::shared_ptr<WebSocketHandler> handler, ByteArray data);
    void handleMessage(Byte opcode, ByteArray data);
    void onAbort(std::shared_ptr<WebSocketHandler> handler);

    Timeout _waiting;
    bool _finalFrame{false};
    Byte _frameOpcode{0};
    std::array<Byte, 4> _frameMask;
    uint64_t _frameLength{0};
    ByteArray _fragmentedMessageBuffer;
    Byte _fragmentedMessageOpcode{0};
    bool _startedClosingHandshake{false};
};

#endif //TINYCORE_WEBSOCKET_H
