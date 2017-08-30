//
// Created by yuwenyong on 17-5-5.
//

#ifndef TINYCORE_WEBSOCKET_H
#define TINYCORE_WEBSOCKET_H

#include "tinycore/common/common.h"
#include <boost/optional.hpp>
#include "tinycore/asyncio/httpclient.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/web.h"
#include "tinycore/common/errors.h"


#define _NOT_SUPPORTED(func) void func() { ThrowException(Exception, #func " not supported for Web Sockets"); }


DECLARE_EXCEPTION(WebSocketError, Exception);

class WebSocketProtocol;


class TC_COMMON_API WebSocketHandler: public RequestHandler {
public:
    friend class WebSocketProtocol;

    WebSocketHandler(Application *application, std::shared_ptr<HTTPServerRequest> request);

    ~WebSocketHandler();

    void writeMessage(const Byte *message, size_t length, bool binary=true);

    void writeMessage(const char *message, bool binary=false) {
        writeMessage((const Byte *)message, strlen(message), binary);
    }

    void writeMessage(const std::string &message, bool binary=false) {
        writeMessage((const Byte *)message.data(), message.size(), binary);
    }

    void writeMessage(const ByteArray &message, bool binary=true) {
        writeMessage(message.data(), message.size(), binary);
    }

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

    void ping(const char *data= nullptr) {
        ping((const Byte *)data, data ? strlen(data) : 0);
    }

    void ping(const std::string &data) {
        ping((const Byte *)data.data(), data.size());
    }

    void ping(const ByteArray &data) {
        ping(data.data(), data.size());
    }

    virtual void onPong(ByteArray data);

    virtual void onClose();

    void close();

    virtual bool allowDraft76() const;

    void setNodelay(bool value) {
        _stream->setNodelay(value);
    }

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
    explicit WebSocketProtocol(WebSocketHandler *handler)
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
    explicit WebSocketProtocol76(WebSocketHandler *handler)
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
    explicit WebSocketProtocol13(WebSocketHandler *handler, bool maskOutgoing=false)
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
        try {
            _stream->readBytes(2, [this, handler=_handler->shared_from_this()](ByteArray data) {
                onFrameStart(std::move(data));
            });
        } catch (StreamClosedError &e) {
            abort();
        }
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


class WebSocketClientProtocol;


class TC_COMMON_API WebSocketClientConnection: public _HTTPConnection {
public:
    friend class WebSocketClientProtocol;

    using ConnectCallbackType = std::function<void (std::shared_ptr<WebSocketClientConnection>)>;
    using ReadCallbackType = std::function<void (boost::optional<ByteArray>)>;
    using SimpleJSONType = boost::property_tree::ptree;

    WebSocketClientConnection(IOLoop *ioloop, std::shared_ptr<HTTPRequest> request, ConnectCallbackType callback);

    ~WebSocketClientConnection();

    std::shared_ptr<HTTPRequest> getRequest() const {
        return _request;
    }

    std::shared_ptr<BaseIOStream> getStream() const {
        return _stream;
    }

    std::exception_ptr getError() const {
        return _error;
    }

    void close();

    void writeMessage(const Byte *message, size_t length, bool binary=true);

    void writeMessage(const char *message, bool binary=false) {
        writeMessage((const Byte *)message, strlen(message), binary);
    }

    void writeMessage(const std::string &message, bool binary=false) {
        writeMessage((const Byte *)message.data(), message.size(), binary);
    }

    void writeMessage(const ByteArray &message, bool binary=true) {
        writeMessage(message.data(), message.size(), binary);
    }

    void writeMessage(const SimpleJSONType &message, bool binary=false) {
        writeMessage(String::fromJSON(message), binary);
    }

#ifdef HAS_RAPID_JSON
    void writeMessage(const rapidjson::Document &message, bool binary=false) {
        writeMessage(String::fromJSON(message), binary);
    }
#endif

    void readMessage(ReadCallbackType callback);
protected:
    void prepare() override;

    void handle1xx(int code) override;

    void onClose() override;

    void onHTTPResponse(HTTPResponse response);

    void onMessage(boost::optional<ByteArray> message);

    void onPong(ByteArray data) {

    }

    ConnectCallbackType _connectCallback;
    ReadCallbackType _readCallback;
    std::deque<boost::optional<ByteArray>> _readQueue;
    std::string _key;
    std::unique_ptr<WebSocketClientProtocol> _protocol;
    std::exception_ptr _error;
};


void WebSocketConnect(const std::string &url, WebSocketClientConnection::ConnectCallbackType callback,
                      float connectTimeout=20.0f, IOLoop *ioloop= nullptr);


class TC_COMMON_API WebSocketClientProtocol {
public:
    friend class WebSocketClientConnection;

    class ReadCallbackWrapper {
    public:
        explicit ReadCallbackWrapper(WebSocketClientProtocol *protocol)
                : _protocol(protocol)
                , _needClear(true) {

        }

        ReadCallbackWrapper(ReadCallbackWrapper &&rhs) noexcept
                : _protocol(rhs._protocol)
                , _needClear(rhs._needClear) {
            rhs._needClear = false;
        }

        ReadCallbackWrapper& operator=(ReadCallbackWrapper &&rhs) noexcept {
            _protocol = std::move(rhs._protocol);
            _needClear = rhs._needClear;
            rhs._needClear = false;
            return *this;
        }

        ~ReadCallbackWrapper() {
            if (_needClear) {
                _protocol->_handler->_readCallback = nullptr;
            }
        }

        void onFrameStart(ByteArray data) {
            _needClear = false;
            _protocol->onFrameStart(std::move(data));
        }

        void onFrameLength16(ByteArray data) {
            _needClear = false;
            _protocol->onFrameLength16(std::move(data));
        }

        void onFrameLength64(ByteArray data) {
            _needClear = false;
            _protocol->onFrameLength64(std::move(data));
        }

        void onMaskingKey(ByteArray data) {
            _needClear = false;
            _protocol->onMaskingKey(std::move(data));
        }

        void onMaskedFrameData(ByteArray data) {
            _needClear = false;
            _protocol->onMaskedFrameData(std::move(data));
        }

        void onFrameData(ByteArray data) {
            _needClear = false;
            _protocol->onFrameData(std::move(data));
        }
    protected:
        WebSocketClientProtocol *_protocol;
        bool _needClear;
    };

    explicit WebSocketClientProtocol(WebSocketClientConnection *handler, bool maskOutgoing=false)
            : _handler(handler)
            , _request(handler->getRequest().get())
            , _stream(handler->getStream().get())
            , _maskOutgoing(maskOutgoing) {

    }

    void setClientTerminated(bool clientTerminated) {
        _clientTerminated = clientTerminated;
    }

    bool getClientTerminated() const {
        return _clientTerminated;
    }

    void writeMessage(const Byte *message, size_t length, bool binary) {
        Byte opcode = (Byte)(binary ? 0x02 : 0x01);
        try {
            writeFrame(true, opcode, message, length);
        } catch (StreamClosedError &e) {
            abort();
        }
    }

    void writePing(const Byte *data, size_t length) {
        writeFrame(true, 0x09, data, length);
    }

    void close();

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

    std::string challengeResponse() const {
        return WebSocketProtocol13::computeAcceptValue(_request->headers().get("Sec-Websocket-Key"));
    }

    void writeFrame(bool fin, Byte opcode, const Byte *data, size_t length);

    void receiveFrame() {
        try {
            auto wrapper = std::make_shared<ReadCallbackWrapper>(this);
            _stream->readBytes(2, std::bind(&ReadCallbackWrapper::onFrameStart, std::move(wrapper), std::placeholders::_1));
        } catch (StreamClosedError &e) {
            abort();
        }
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

    WebSocketClientConnection *_handler{nullptr};
    HTTPRequest *_request{nullptr};
    BaseIOStream *_stream{nullptr};
    bool _clientTerminated{false};
    bool _serverTerminated{false};

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
