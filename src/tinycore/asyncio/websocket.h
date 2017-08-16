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

    void writeMessage(const Byte *message, size_t length, bool binary=true);

    void writeMessage(const char *message, bool binary=false);

    void writeMessage(const std::string &message, bool binary=false);

    void writeMessage(const ByteArray &message, bool binary=true);

    void writeMessage(const SimpleJSONType &message, bool binary=false) {
        writeMessage(String::fromJSON(message), binary);
    }

#ifdef HAS_RAPID_JSON
    void writeMessage(const rapidjson::Document &message, bool binary=false) {
        writeMessage(String::fromJSON(message), binary);
    }
#endif

    virtual boost::optional<std::string> selectSubProtocol(const StringVector &subProtocols) const;

    void open() {
        onOpen(_openArgs);
    }

    virtual void onOpen(const StringVector &args);
    virtual void onMessage(ByteArray data) = 0;

    void ping(const Byte *data, size_t length);

    void ping(const char *data= nullptr);

    void ping(const std::string &data);

    void ping(const ByteArray &data);

    virtual void onPong(ByteArray data);

    virtual void onClose();

    void close();
    virtual bool allowDraft76() const;

    std::string getWebSocketScheme() const {
        return _request->getProtocol() == "https" ? "wss" : "ws";
    }

    void onConnectionClose() override;
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
    WebSocketProtocol(WebSocketHandler *handler)
            : _handler(handler)
            , _request(handler->getRequest().get())
            , _stream(handler->_stream.get()) {

    }

    virtual ~WebSocketProtocol() {}

    void setClientTerminated(bool clientTerminated) {
        _clientTerminated = clientTerminated;
    }

    bool getClientTerminated() const {
        return _clientTerminated;
    }

    virtual void acceptConnection() = 0;
    virtual void writeMessage(const Byte *message, size_t length, bool binary) = 0;
    virtual void writePing(const Byte *data, size_t length) = 0;
    virtual void close() = 0;

    void onConnectionClose() {
        abort();
    }
protected:
    void abort() {
        _clientTerminated = true;
        _serverTerminated = true;
        _stream->close();
        close();
    }

    WebSocketHandler *_handler{nullptr};
    HTTPServerRequest *_request{nullptr};
    BaseIOStream *_stream{nullptr};
    bool _clientTerminated{false};
    bool _serverTerminated{false};
};


class TC_COMMON_API WebSocketProtocol76: public WebSocketProtocol {
public:
    WebSocketProtocol76(WebSocketHandler *handler)
            : WebSocketProtocol(handler) {

    }

    void acceptConnection() override;
    std::string challengeResponse(const std::string &challenge) const;
    void writeMessage(const Byte *message, size_t length, bool binary) override;
    void writePing(const Byte *data, size_t length) override;
    void close() override;
protected:
    void handleChallenge(ByteArray data);
    void writeResponse(const std::string &challenge);
    void handleWebSocketHeaders() const;
    std::string calculatePart(const std::string &key) const;
    std::string generateChallengeResponse(const std::string &part1, const std::string &part2,
                                          const std::string &part3) const;

    void receiveMessage() {
        _stream->readBytes(1, [this, handler=_handler->shared_from_this()](ByteArray data) {
           onFrameType(std::move(data));
        });
    }

    void onFrameType(ByteArray data);
    void onEndDelimiter(ByteArray data);
    void onLengthIndicator(ByteArray data);

//    std::string _challenge;
    Timeout _waiting;
};


class TC_COMMON_API WebSocketProtocol13: public WebSocketProtocol {
public:
    WebSocketProtocol13(WebSocketHandler *handler, bool maskOutgoing=false)
            : WebSocketProtocol(handler)
            , _maskOutgoing(maskOutgoing) {

    }

    void acceptConnection() override;
    void writeMessage(const Byte *message, size_t length, bool binary) override;
    void writePing(const Byte *data, size_t length) override;
    void close() override;

    static std::string computeAcceptValue(const std::string &key);
protected:
    void handleWebSocketHeaders() const;

    std::string challengeResponse() const {
        return computeAcceptValue(_request->getHTTPHeaders()->get("Sec-Websocket-Key"));
    }

    void _acceptConnection();

    void writeFrame(bool fin, Byte opcode, const Byte *data, size_t length);

    void receiveFrame() {
        _stream->readBytes(2, [this, handler=_handler->shared_from_this()](ByteArray data) {
            onFrameStart(std::move(data));
        });
    }

    void onFrameStart(ByteArray data);

    void onFrameLength16(ByteArray data);

    void onFrameLength64(ByteArray data);

    void onMaskingKey(ByteArray data);

    void applyMask(const std::array<Byte, 4> &mask, Byte *data, size_t length) {
        for (size_t i = 0; i != length; ++i) {
            data[i] ^= mask[i % 4];
        }
    }

    void onMaskedFrameData(ByteArray data) {
        applyMask(_frameMask, data.data(), data.size());
        onFrameData(std::move(data));
    }

    void onFrameData(ByteArray data);

    void handleMessage(Byte opcode, ByteArray data);

    bool _maskOutgoing{false};
    bool _finalFrame{false};
    bool _frameOpcodeIsControl{false};
    bool _maskedFrame{false};
    Byte _frameOpcode{0};
    std::array<Byte, 4> _frameMask;
    uint64_t _frameLength{0};
    ByteArray _fragmentedMessageBuffer;
    Byte _fragmentedMessageOpcode{0};
    Timeout _waiting;
};





#endif //TINYCORE_WEBSOCKET_H
