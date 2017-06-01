//
// Created by yuwenyong on 17-5-17.
//

#ifndef TINYCORE_HTTPCLIENT_H
#define TINYCORE_HTTPCLIENT_H

#include "tinycore/common/common.h"
#include <chrono>
#include <boost/optional.hpp>
#include <boost/parameter.hpp>
#include "tinycore/asyncio/httputil.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/web.h"
#include "tinycore/compress/zlib.h"
#include "tinycore/httputils/httplib.h"
#include "tinycore/utilities/string.h"


namespace opts {
BOOST_PARAMETER_NAME(url)
BOOST_PARAMETER_NAME(method)
BOOST_PARAMETER_NAME(headers)
BOOST_PARAMETER_NAME(body)
BOOST_PARAMETER_NAME(authUserName)
BOOST_PARAMETER_NAME(authPassword)
BOOST_PARAMETER_NAME(connectTimeout)
BOOST_PARAMETER_NAME(requestTimeout)
BOOST_PARAMETER_NAME(ifModifiedSince)
BOOST_PARAMETER_NAME(followRedirects)
BOOST_PARAMETER_NAME(maxRedirects)
BOOST_PARAMETER_NAME(userAgent)
BOOST_PARAMETER_NAME(useGzip)
BOOST_PARAMETER_NAME(networkInterface)
BOOST_PARAMETER_NAME(streamCallback)
BOOST_PARAMETER_NAME(headerCallback)
//BOOST_PARAMETER_NAME(prepareCurlCallback)
BOOST_PARAMETER_NAME(proxyHost)
BOOST_PARAMETER_NAME(proxyPort)
BOOST_PARAMETER_NAME(proxyUserName)
BOOST_PARAMETER_NAME(proxyPassword)
BOOST_PARAMETER_NAME(allowNonstandardMethods)
BOOST_PARAMETER_NAME(validateCert)
BOOST_PARAMETER_NAME(caCerts)
BOOST_PARAMETER_NAME(request)
BOOST_PARAMETER_NAME(code)
BOOST_PARAMETER_NAME(effectiveURL)
BOOST_PARAMETER_NAME(error)
}

#define url_ opts::_url
#define method_ opts::_method
#define headers_ opts::_headers
#define body_ opts::_body
#define authUserName_ opts::_authUserName
#define authPassword_ opts::_authPassword
#define connectTimeout_ opts::_connectTimeout
#define requestTimeout_ opts::_requestTimeout
#define ifModifiedSince_ opts::_ifModifiedSince
#define followRedirects_ opts::_followRedirects
#define maxRedirects_ opts::_maxRedirects
#define userAgent_ opts::_userAgent
#define useGzip_ opts::_useGzip
#define networkInterface_ opts::_networkInterface
#define streamCallback_ opts::_streamCallback
#define headerCallback_ opts::_headerCallback
#define proxyHost_ opts::_proxyHost
#define proxyPort_ opts::_proxyPort
#define proxyUserName_ opts::_proxyUserName
#define proxyPassword_ opts::_proxyPassword
#define allowNonstandardMethods_ opts::_allowNonstandardMethods
#define validateCert_ opts::_validateCert
#define caCerts_ opts::_caCerts
#define request_ opts::_request
#define code_ opts::_code
#define effectiveURL_ opts::_effectiveURL
#define error_ opts::_error


class HTTPRequestImpl {
public:
    typedef std::function<void (const ByteArray &)> StreamCallbackType;
    typedef std::function<void (const std::string &)> HeaderCallbackType;

    template <typename ArgumentPack>
    HTTPRequestImpl(ArgumentPack const& args) {
        _headers = args[opts::_headers | HTTPHeaders()];
        boost::optional<DateTime> ifModifiedSince = args[opts::_ifModifiedSince | boost::none];
        if (ifModifiedSince) {
            std::string timestamp = String::formatUTCDate(ifModifiedSince.get(), true);
            _headers["If-Modified-Since"] = timestamp;
        }
        if (!_headers.has("Pragma")) {
            _headers["Pragma"] = "";
        }
        _proxyHost = args[opts::_proxyHost | ""];
        _proxyPort = args[opts::_proxyPort | 0];
        _proxyUserName = args[opts::_proxyUserName | ""];
        _proxyPassword = args[opts::_proxyPassword | ""];
        if (!_headers.has("Expect")) {
            _headers["Expect"] = "";
        }
        _url = args[opts::_url];
        _method = args[opts::_method | "GET"];
        _body = args[opts::_body | boost::none];
        _authUserName = args[opts::_authUserName | ""];
        _authPassword = args[opts::_authPassword | ""];
        _connectTimeout = args[opts::_connectTimeout | 20.0f];
        _requestTimeout = args[opts::_requestTimeout | 20.0f];
        _followRedirects = args[opts::_followRedirects | true];
        _maxRedirects = args[opts::_maxRedirects | 5];
        _userAgent = args[opts::_userAgent | ""];
        _useGzip = args[opts::_useGzip | true];
        _networkInterface = args[opts::_networkInterface | ""];
        _streamCallback = args[opts::_streamCallback | nullptr];
        _headerCallback = args[opts::_headerCallback | nullptr];
        _allowNonstandardMethods = args[opts::_allowNonstandardMethods | false];
        _validateCert = args[opts::_validateCert | true];
        _caCerts = args[opts::_caCerts | ""];
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

    void setBody(ByteArray body) {
        _body = std::move(body);
    }

    const ByteArray* getBody() const {
        return _body.get_ptr();
    }

    void setAuthUserName(std::string authUserName) {
        _authUserName = std::move(authUserName);
    }

    const std::string& getAuthUserName() const {
        return _authUserName;
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

    void decreaseRedirects() {
        --_maxRedirects;
    }

    void setUserAgent(std::string userAgent) {
        _userAgent = std::move(userAgent);
    }

    const std::string& getUserAgent() const {
        return _userAgent;
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

    const std::string& getNetworkInterface() const {
        return _networkInterface;
    }

    void setStreamCallback(StreamCallbackType streamCallback) {
        _streamCallback = std::move(streamCallback);
    }

    const StreamCallbackType& getStreamCallback() const {
        return _streamCallback;
    }

    void setHeaderCallback(HeaderCallbackType headerCallback) {
        _headerCallback = std::move(headerCallback);
    }

    const HeaderCallbackType& getHeaderCallback() const {
        return _headerCallback;
    }

    void setProxyHost(std::string proxyHost) {
        _proxyHost = std::move(proxyHost);
    }

    const std::string& getProxyHost() const {
        return _proxyHost;
    }

    void setProxyPort(unsigned short proxyPort) {
        _proxyPort = proxyPort;
    }

    unsigned short getProxyPort() const {
        return _proxyPort;
    }

    void setProxyUserName(std::string proxyUserName) {
        _proxyUserName = std::move(proxyUserName);
    }

    const std::string& getProxyUserName() const {
        return _proxyUserName;
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

    const std::string& getCACerts() const {
        return _caCerts;
    }
protected:
    std::string _url;
    std::string _method;
    HTTPHeaders _headers;
    boost::optional<ByteArray> _body;
    std::string _authUserName;
    std::string _authPassword;
    float _connectTimeout;
    float _requestTimeout;
    bool _followRedirects;
    int _maxRedirects;
    std::string _userAgent;
    bool _useGzip;
    std::string _networkInterface;
    StreamCallbackType _streamCallback;
    HeaderCallbackType _headerCallback;
    std::string _proxyHost;
    unsigned short _proxyPort;
    std::string _proxyUserName;
    std::string _proxyPassword;
    bool _allowNonstandardMethods;
    bool _validateCert;
    std::string _caCerts;
    Timestamp _startTime;
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
                    (body, (ByteArray))
                    (authUserName, (std::string))
                    (authPassword, (std::string))
                    (connectTimeout, (float))
                    (requestTimeout, (float))
                    (ifModifiedSince, (DateTime))
                    (followRedirects, (bool))
                    (maxRedirects, (int))
                    (userAgent, (std::string))
                    (useGzip, (bool))
                    (networkInterface, (std::string))
                    (streamCallback, (StreamCallbackType))
                    (headerCallback, (HeaderCallbackType))
                    (proxyHost, (std::string))
                    (proxyPort, (unsigned short))
                    (proxyUserName, (std::string))
                    (proxyPassword, (std::string))
                    (allowNonstandardMethods, (bool))
                    (validateCert, (bool))
                    (caCerts, (std::string))
    ))

    static std::shared_ptr<HTTPRequest> create(std::shared_ptr<HTTPRequest> request) {
        return std::make_shared<HTTPRequest>(*request);
    }

    template <typename ...Args>
    static std::shared_ptr<HTTPRequest> create(Args&& ...args) {
        return std::make_shared<HTTPRequest>(std::forward<Args>(args)...);
    }
};


class HTTPResponseImpl {
public:
    template <typename ArgumentPack>
    HTTPResponseImpl(ArgumentPack const& args) {
        _request = args[opts::_request];
        _code = args[opts::_code];
        _headers = args[opts::_headers | HTTPHeaders()];
        _body = args[opts::_body | boost::none];
        _effectiveURL = args[opts::_effectiveURL | _request->getURL()];
        _error = args[opts::_error | boost::none];
        if (!_error) {
            if (_code < 200 || _code >= 300) {
                auto iter = HTTPResponses.find(_code);
                if (iter != HTTPResponses.end()) {
                    _error = iter->second;
                }
            }
        }
    }

    std::shared_ptr<HTTPRequest> getRequest() const {
        return _request;
    }

    int getCode() const {
        return _code;
    }

    const HTTPHeaders& getHeaders() const {
        return _headers;
    }

    const ByteArray* getBody() const {
        return _body.get_ptr();
    }

    const std::string& getEffectiveURL() const {
        return _effectiveURL;
    }

    const std::string* getError() const {
        return _error.get_ptr();
    }

    void rethrow() {
        if (_error) {
            ThrowException(HTTPError, _code, _error.get());
        }
    }
protected:
    std::shared_ptr<HTTPRequest> _request;
    int _code;
    HTTPHeaders _headers;
    boost::optional<ByteArray> _body;
    std::string _effectiveURL;
    boost::optional<std::string> _error;
};


class HTTPResponse: public HTTPResponseImpl {
public:
    BOOST_PARAMETER_CONSTRUCTOR(HTTPResponse, (HTTPResponseImpl), opts::tag, (
            required
                    (request, (std::shared_ptr<HTTPRequest>))
                    (code, (int))
    )(
            optional
                    (headers, (HTTPHeaders))
                    (body, (ByteArray))
                    (effectiveURL, (std::string))
                    (error, (std::string))
    ))
};


class HTTPClient: public std::enable_shared_from_this<HTTPClient> {
public:
    typedef std::function<void (const HTTPResponse&)> CallbackType;

    HTTPClient(IOLoop *ioloop=nullptr, StringMap hostnameMapping={});
    ~HTTPClient();

    void close() {

    }

    template <typename ...Args>
    void fetch(const std::string &url, CallbackType callback, Args&& ...args) {
        fetch(HTTPRequest::create(url, std::forward<Args>(args)...), std::move(callback));
    }

    void fetch(std::shared_ptr<HTTPRequest> request, CallbackType callback) {
        fetch(request, request, std::move(callback));
    }

    void fetch(std::shared_ptr<HTTPRequest> originalRequest, std::shared_ptr<HTTPRequest> request, CallbackType callback);

    template <typename ...Args>
    static std::shared_ptr<HTTPClient> create(Args&& ...args) {
        return std::make_shared<HTTPClient>(std::forward<Args>(args)...);
    }

    const StringMap& getHostnameMapping() const {
        return _hostnameMapping;
    }
protected:
    void onFetchComplete(CallbackType callback, const HTTPResponse &response) {
        callback(response);
    }

    IOLoop * _ioloop;
    StringMap _hostnameMapping;
};


class _HTTPConnection: public std::enable_shared_from_this<_HTTPConnection> {
public:
    typedef HTTPClient::CallbackType CallbackType;

    _HTTPConnection(IOLoop *ioloop, std::shared_ptr<HTTPClient> client, std::shared_ptr<HTTPRequest> originalRequest,
                    std::shared_ptr<HTTPRequest> request, CallbackType callback);
    ~_HTTPConnection();
    void start();

    template <typename ...Args>
    static std::shared_ptr<_HTTPConnection> create(Args&& ...args) {
        return std::make_shared<_HTTPConnection>(std::forward<Args>(args)...);
    }

    static const StringSet supportedMethods;
protected:
    std::shared_ptr<BaseIOStream> fetchStream() {
        return _streamObserver.lock();
    }

    void onTimeout();
    void onConnect();
    void onClose();
    void onHeaders(Byte *data, size_t length);
    void onBody(Byte *data, size_t length);
    void onChunkLength(Byte *data, size_t length);
    void onChunkData(Byte *data, size_t length);

    Timestamp _startTime;
    IOLoop *_ioloop;
    std::shared_ptr<HTTPClient> _client;
    std::shared_ptr<HTTPRequest> _originalRequest;
    std::shared_ptr<HTTPRequest> _request;
    CallbackType _callback;
    boost::optional<int> _code;
    std::unique_ptr<HTTPHeaders> _headers;
    boost::optional<ByteArray> _chunks;
    std::unique_ptr<DecompressObj> _decompressor;
    Timeout _timeout;
    Timeout _connectTimeout;
    std::weak_ptr<BaseIOStream> _streamObserver;
    std::shared_ptr<BaseIOStream> _stream;
    std::string _parsedScheme;
    std::string _parsedNetloc;
    std::string _parsedPath;
    std::string _parsedQuery;
    std::string _parsedFragment;
};

#endif //TINYCORE_HTTPCLIENT_H
