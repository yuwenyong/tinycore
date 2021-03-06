//
// Created by yuwenyong on 17-5-17.
//

#ifndef TINYCORE_HTTPCLIENT_H
#define TINYCORE_HTTPCLIENT_H

#include "tinycore/common/common.h"
#include "tinycore/common/params.h"
#include <chrono>
#include <boost/optional.hpp>
#include "tinycore/asyncio/httputil.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/httputils/httplib.h"
#include "tinycore/httputils/urlparse.h"
#include "tinycore/utilities/string.h"


class HTTPRequestImpl {
public:
    typedef std::function<void (ByteArray)> StreamingCallbackType;
    typedef std::function<void (std::string)> HeaderCallbackType;

    template <typename ArgumentPack>
    HTTPRequestImpl(ArgumentPack const& args) {
        _headers = args[opts::_headers | HTTPHeaders()];
        boost::optional<DateTime> ifModifiedSince = args[opts::_ifModifiedSince | boost::none];
        if (ifModifiedSince) {
            _headers["If-Modified-Since"] = HTTPUtil::formatTimestamp(ifModifiedSince.get());
        }
//        if (!_headers.has("Pragma")) {
//            _headers["Pragma"] = "";
//        }
        _proxyHost = args[opts::_proxyHost | boost::none];
        _proxyPort = args[opts::_proxyPort | boost::none];
        _proxyUserName = args[opts::_proxyUserName | boost::none];
        _proxyPassword = args[opts::_proxyPassword | ""];
//        if (!_headers.has("Expect")) {
//            _headers["Expect"] = "";
//        }
        _url = args[opts::_url];
        _method = args[opts::_method | "GET"];
        _body = args[opts::_body | boost::none];
        _authUserName = args[opts::_authUserName | boost::none];
        _authPassword = args[opts::_authPassword | ""];
        _authMode = args[opts::_authMode | boost::none];
        _connectTimeout = args[opts::_connectTimeout | 20.0f];
        _requestTimeout = args[opts::_requestTimeout | 20.0f];
        _followRedirects = args[opts::_followRedirects | true];
        _maxRedirects = args[opts::_maxRedirects | 5];
        _userAgent = args[opts::_userAgent | boost::none];
        _useGzip = args[opts::_useGzip | true];
        _networkInterface = args[opts::_networkInterface | boost::none];
        _streamingCallback = args[opts::_streamingCallback | nullptr];
        _streamingCallback = StackContext::wrap<ByteArray>(std::move(_streamingCallback));
        _headerCallback = args[opts::_headerCallback | nullptr];
        _headerCallback = StackContext::wrap<std::string>(std::move(_headerCallback));
        _allowNonstandardMethods = args[opts::_allowNonstandardMethods | false];
        _validateCert = args[opts::_validateCert | true];
        _caCerts = args[opts::_caCerts | boost::none];
        _clientKey = args[opts::_clientKey | boost::none];
        _clientCert = args[opts::_clientCert | boost::none];
        _startTime = TimestampClock::now();
    }

    void setURL(std::string url) {
        _url = std::move(url);
    }

    const std::string& getURL() const {
        return _url;
    }

    void setMethod(std::string method) {
        _method = std::move(method);
    }

    const std::string& getMethod() const {
        return _method;
    }

    void setHeaders(HTTPHeaders headers) {
        _headers = std::move(headers);
    }

    HTTPHeaders& headers() {
        return _headers;
    }

    void setBody(boost::optional<std::string> body) {
        _body = std::move(body);
    }

    const std::string* getBody() const {
        return _body.get_ptr();
    }

    void setAuthUserName(std::string authUserName) {
        _authUserName = std::move(authUserName);
    }

    const std::string* getAuthUserName() const {
        return _authUserName.get_ptr();
    }

    void setAuthMode(std::string authMode) {
        _authMode = std::move(authMode);
    }

    const std::string* getAuthMode() const {
        return _authMode.get_ptr();
    }

    void setAuthPassword(std::string authPassword) {
        _authPassword = std::move(authPassword);
    }

    const std::string& getAuthPassword() const {
        return _authPassword;
    }

    void setConnectTimeout(float connectTimeout) {
        _connectTimeout = connectTimeout;
    }

    float getConnectTimeout() const {
        return _connectTimeout;
    }

    void setRequestTimeout(float requestTimeout) {
        _requestTimeout = requestTimeout;
    }

    float getRequestTimeout() const {
        return _requestTimeout;
    }

    void setFollowRedirects(bool followRedirects) {
        _followRedirects = followRedirects;
    }

    bool isFollowRedirects() const {
        return _followRedirects;
    }

    void setMaxRedirects(int maxRedirects) {
        _maxRedirects = maxRedirects;
    }

    int getMaxRedirects() const {
        return _maxRedirects;
    }

    void setUserAgent(std::string userAgent) {
        _userAgent = std::move(userAgent);
    }

    const std::string* getUserAgent() const {
        return _userAgent.get_ptr();
    }

    void setUseGzip(bool useGzip) {
        _useGzip = useGzip;
    }

    bool isUseGzip() const {
        return _useGzip;
    }

    void setNetworkInterface(std::string networkInterface) {
        _networkInterface = std::move(networkInterface);
    }

    const std::string* getNetworkInterface() const {
        return _networkInterface.get_ptr();
    }

    void setStreamingCallback(StreamingCallbackType streamingCallback) {
        _streamingCallback = StackContext::wrap<ByteArray>(std::move(streamingCallback));
    }

    const StreamingCallbackType& getStreamingCallback() const {
        return _streamingCallback;
    }

    void setHeaderCallback(HeaderCallbackType headerCallback) {
        _headerCallback = StackContext::wrap<std::string>(std::move(headerCallback));
    }

    const HeaderCallbackType& getHeaderCallback() const {
        return _headerCallback;
    }

    void setProxyHost(std::string proxyHost) {
        _proxyHost = std::move(proxyHost);
    }

    const std::string* getProxyHost() const {
        return _proxyHost.get_ptr();
    }

    void setProxyPort(unsigned short proxyPort) {
        _proxyPort = proxyPort;
    }

    const unsigned short* getProxyPort() const {
        return _proxyPort.get_ptr();
    }

    void setProxyUserName(std::string proxyUserName) {
        _proxyUserName = std::move(proxyUserName);
    }

    const std::string* getProxyUserName() const {
        return _proxyUserName.get_ptr();
    }

    void setProxyPassword(std::string proxyPassword) {
        _proxyPassword = std::move(proxyPassword);
    }

    const std::string& getProxyPassword() const {
        return _proxyPassword;
    }

    void setAllowNonstandardMethods(bool allowNonstandardMethods) {
        _allowNonstandardMethods = allowNonstandardMethods;
    }

    bool isAllowNonstandardMethods() const {
        return _allowNonstandardMethods;
    }

    void setValidateCert(bool validateCert) {
        _validateCert = validateCert;
    }

    bool isValidateCert() const {
        return _validateCert;
    }

    void setCACerts(std::string caCerts) {
        _caCerts = std::move(caCerts);
    }

    const std::string* getCACerts() const {
        return _caCerts.get_ptr();
    }

    void setClientKey(std::string clientKey) {
        _clientKey = std::move(clientKey);
    }

    const std::string* getClientKey() const {
        return _clientKey.get_ptr();
    }

    void setClientCert(std::string clientCert) {
        _clientCert = std::move(clientCert);
    }

    const std::string* getClientCert() const {
        return _clientCert.get_ptr();
    }
protected:
    std::string _url;
    std::string _method;
    HTTPHeaders _headers;
    boost::optional<std::string> _body;
    boost::optional<std::string> _authUserName;
    std::string _authPassword;
    boost::optional<std::string> _authMode;
    float _connectTimeout;
    float _requestTimeout;
    bool _followRedirects;
    int _maxRedirects;
    boost::optional<std::string> _userAgent;
    bool _useGzip;
    boost::optional<std::string> _networkInterface;
    StreamingCallbackType _streamingCallback;
    HeaderCallbackType _headerCallback;
    boost::optional<std::string> _proxyHost;
    boost::optional<unsigned short> _proxyPort;
    boost::optional<std::string> _proxyUserName;
    std::string _proxyPassword;
    bool _allowNonstandardMethods;
    bool _validateCert;
    boost::optional<std::string> _caCerts;
    boost::optional<std::string> _clientKey;
    boost::optional<std::string> _clientCert;
    Timestamp _startTime;
};


class TC_COMMON_API _HTTPError: public Exception {
public:
    _HTTPError(const char *file, int line, const char *func, int code)
            : _HTTPError(file, line, func, code, HTTPUtil::getHTTPReason(code)) {

    }

    _HTTPError(const char *file, int line, const char *func, int code, const std::string &message)
            : Exception(file, line, func, message)
            , _code(code) {

    }

    const char *what() const noexcept override;

    const char *getTypeName() const override {
        return "HTTPError";
    }

    int getCode() const {
        return _code;
    }
protected:
    int _code;
};


class HTTPRequest: public HTTPRequestImpl {
public:
    BOOST_PARAMETER_CONSTRUCTOR(HTTPRequest, (HTTPRequestImpl), opts::tag, (
            required
                    (url, (std::string))
    )(
            optional
                    (method, (std::string))
                    (headers, (HTTPHeaders))
                    (body, (std::string))
                    (authUserName, (std::string))
                    (authPassword, (std::string))
                    (authMode, (std::string))
                    (connectTimeout, (float))
                    (requestTimeout, (float))
                    (ifModifiedSince, (DateTime))
                    (followRedirects, (bool))
                    (maxRedirects, (int))
                    (userAgent, (std::string))
                    (useGzip, (bool))
                    (networkInterface, (std::string))
                    (streamingCallback, (StreamingCallbackType))
                    (headerCallback, (HeaderCallbackType))
                    (proxyHost, (std::string))
                    (proxyPort, (unsigned short))
                    (proxyUserName, (std::string))
                    (proxyPassword, (std::string))
                    (allowNonstandardMethods, (bool))
                    (validateCert, (bool))
                    (caCerts, (std::string))
                    (clientKey, (std::string))
                    (clientCert, (std::string))
    ))

    void setOriginalRequest(std::shared_ptr<HTTPRequest> originalRequest) {
        _originalRequest = std::move(originalRequest);
    }

    std::shared_ptr<HTTPRequest> getOriginalRequest() const {
        return _originalRequest;
    }

    static std::shared_ptr<HTTPRequest> create(std::shared_ptr<HTTPRequest> request) {
        return std::make_shared<HTTPRequest>(*request);
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPRequest> create(Args&& ...args) {
        return std::make_shared<HTTPRequest>(std::forward<Args>(args)...);
    }
protected:
    std::shared_ptr<HTTPRequest> _originalRequest;  
};


class HTTPResponse {
public:
    HTTPResponse() = default; 
    
    HTTPResponse(std::shared_ptr<HTTPRequest> request, int code, std::string reason, HTTPHeaders headers,
                 std::string body, std::string effectiveURL, Duration requestTime)
            : _request(std::move(request))
            , _code(code)
            , _headers(std::move(headers))
            , _body(std::move(body))
            , _effectiveURL(std::move(effectiveURL))
            , _requestTime(std::move(requestTime)) {
        if (reason.empty()) {
            _reason = HTTPUtil::getHTTPReason(code);
        } else {
            _reason = std::move(reason);
        }
        if (!_error) {
            if (_code < 200 || _code >= 300) {
                _error = MakeExceptionPtr(_HTTPError, _code);
            }
        }
    }

    HTTPResponse(std::shared_ptr<HTTPRequest> request, int code, std::exception_ptr error, Duration requestTime)
            : _request(std::move(request))
            , _code(code)
            , _error(std::move(error))
            , _requestTime(std::move(requestTime)) {
        _reason = HTTPUtil::getHTTPReason(code);
        if (!_error) {
            if (_code < 200 || _code >= 300) {
                _error = MakeExceptionPtr(_HTTPError, _code);
            }
        }
    }

    std::shared_ptr<HTTPRequest> getRequest() const {
        return _request;
    }

    int getCode() const {
        return _code;
    }

    const std::string& getReason() const {
        return _reason;
    }

    const HTTPHeaders& getHeaders() const {
        return _headers;
    }

    const std::string* getBody() const {
        return _body.get_ptr();
    }

    const std::string& getEffectiveURL() const {
        return _effectiveURL;
    }

    std::exception_ptr getError() const {
        return _error;
    }

    const Duration& getRequestTime() const {
        return _requestTime;
    }

    void rethrow() {
        if (_error) {
            std::rethrow_exception(_error);
        }
    }

    std::string toString() const;
protected:
    std::shared_ptr<HTTPRequest> _request;
    int _code{200};
    std::string _reason;
    HTTPHeaders _headers;
    boost::optional<std::string> _body;
    std::string _effectiveURL;
    std::exception_ptr _error;
    Duration _requestTime;
};


class HTTPClient: public std::enable_shared_from_this<HTTPClient> {
public:
    typedef std::function<void (HTTPResponse)> CallbackType;

    HTTPClient(IOLoop *ioloop=nullptr, StringMap hostnameMapping={}, size_t maxBufferSize=DEFAULT_MAX_BUFFER_SIZE);
    ~HTTPClient();

    void close() {

    }

    template <typename ...Args>
    void fetch(const std::string &url, CallbackType callback, Args&& ...args) {
        fetch(HTTPRequest::create(url, std::forward<Args>(args)...), std::move(callback));
    }

    void fetch(const HTTPRequest &request, CallbackType callback) {
        fetch(HTTPRequest::create(request), std::move(callback));
    }

    void fetch(std::shared_ptr<HTTPRequest> request, CallbackType callback);

    const StringMap& getHostnameMapping() const {
        return _hostnameMapping;
    }

    size_t getMaxBufferSize() const {
        return _maxBufferSize;
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPClient> create(Args&& ...args) {
        return std::make_shared<HTTPClient>(std::forward<Args>(args)...);
    }
protected:
    IOLoop * _ioloop;
    StringMap _hostnameMapping;
    size_t _maxBufferSize;
};


class _HTTPConnection: public std::enable_shared_from_this<_HTTPConnection> {
public:
    typedef HTTPClient::CallbackType CallbackType;

    class CloseCallbackWrapper {
    public:
        explicit CloseCallbackWrapper(_HTTPConnection *connection)
                : _connection(connection)
                , _needClear(true) {

        }

        CloseCallbackWrapper(CloseCallbackWrapper &&rhs) noexcept
                : _connection(rhs._connection)
                , _needClear(rhs._needClear) {
            rhs._needClear = false;
        }

        CloseCallbackWrapper& operator=(CloseCallbackWrapper &&rhs) noexcept {
            _connection = rhs._connection;
            _needClear = rhs._needClear;
            rhs._needClear = false;
            return *this;
        }

        ~CloseCallbackWrapper() {
            if (_needClear) {
                _connection->_callback = nullptr;
            }
        }

        void operator()() {
            _needClear = false;
            _connection->onClose();
        }
    protected:
        _HTTPConnection *_connection;
        bool _needClear;
    };

    _HTTPConnection(IOLoop *ioloop, std::shared_ptr<HTTPClient> client, std::shared_ptr<HTTPRequest> request,
                    CallbackType callback, size_t maxBufferSize);

    virtual ~_HTTPConnection();

    void start();

    template <typename ...Args>
    static std::shared_ptr<_HTTPConnection> create(Args&& ...args) {
        return std::make_shared<_HTTPConnection>(std::forward<Args>(args)...);
    }

    template <typename T>
    std::shared_ptr<T> getSelf() {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    template <typename T>
    std::shared_ptr<const T> getSelf() const {
        return std::static_pointer_cast<const T>(shared_from_this());
    }

    static const StringSet supportedMethods;
protected:
    std::shared_ptr<BaseIOStream> createStream() const;

    void onTimeout();

    void removeTimeout();

    void onConnect();

    void runCallback(HTTPResponse response);

    void handleException(std::exception_ptr error);

    virtual void prepare();

    virtual void onClose();

    virtual void handle1xx(int code);

    void onHeaders(ByteArray data);

    void onBody(ByteArray data);

    void onEndRequest() {
        _stream->close();
    }

    void onChunkLength(ByteArray data);

    void onChunkData(ByteArray data);

    Timestamp _startTime;
    IOLoop *_ioloop;
    std::shared_ptr<HTTPClient> _client;
    std::shared_ptr<HTTPRequest> _request;
    CallbackType _callback;
    size_t _maxBufferSize;
    boost::optional<int> _code;
    std::unique_ptr<HTTPHeaders> _headers;
    boost::optional<ByteArray> _chunks;
    std::unique_ptr<GzipDecompressor> _decompressor;
    URLSplitResult _parsed;
    std::string _parsedHostname;
    Timeout _timeout;
    std::shared_ptr<BaseIOStream> _stream;
    std::string _reason;
};

#endif //TINYCORE_HTTPCLIENT_H
