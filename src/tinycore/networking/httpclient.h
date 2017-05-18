//
// Created by yuwenyong on 17-5-17.
//

#ifndef TINYCORE_HTTPCLIENT_H
#define TINYCORE_HTTPCLIENT_H

#include "tinycore/common/common.h"
#include <chrono>
#include <boost/parameter.hpp>
#include <boost/optional.hpp>
#include "tinycore/networking/ioloop.h"
#include "tinycore/networking/httputil.h"
#include "tinycore/networking/iostream.h"
#include "tinycore/networking/web.h"
#include "tinycore/compress/zlib.h"
#include "tinycore/utilities/string.h"
#include "tinycore/utilities/httplib.h"


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
            _headers.setItem("If-Modified-Since", timestamp);
        }
        if (!_headers.contain("Pragma")) {
            _headers.setItem("Pragma", "");
        }
        _proxyHost = args[opts::_proxyHost | ""];
        _proxyPort = args[opts::_proxyPort | 0];
        _proxyUserName = args[opts::_proxyUserName | ""];
        _proxyPassword = args[opts::_proxyPassword | ""];
        if (!_headers.contain("Expect")) {
            _headers.setItem("Expect", "");
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

    const std::string& getURL() const {
        return _url;
    }

    const std::string& getMethod() const {
        return _method;
    }

    float getConnectTimeout() const {
        return _connectTimeout;
    }

    float getRequestTimeout() const {
        return _requestTimeout;
    }

    const std::string& getNetworkInterface() const {
        return _networkInterface;
    }

    const std::string& getProxyHost() const {
        return _proxyHost;
    }

    unsigned short getProxyPort() const {
        return _proxyPort;
    }

    const std::string& getProxyUserName() const {
        return _proxyUserName;
    }

    const std::string& getProxyPassword() const {
        return _proxyPassword;
    }

    bool getAllowNonstandardMethods() const {
        return _allowNonstandardMethods;
    }

    bool getValidateCert() const {
        return _validateCert;
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

typedef std::shared_ptr<HTTPRequest> HTTPRequestPtr;


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

    void rethrow() {
        if (_error) {
            ThrowException(HTTPError, _code, _error.get());
        }
    }
protected:
    HTTPRequestPtr _request;
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
                    (request, (HTTPRequestPtr))
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
    typedef std::function<void (HTTPResponse)> CallbackType;

    HTTPClient(IOLoop *ioloop=nullptr, StringMap hostnameMapping={});

    template <typename ...Args>
    void fetch(const std::string &url, CallbackType callback, Args&& ...args) {
        fetch(HTTPRequest::create(url, std::forward<Args>(args)...), std::move(callback));
    }

    void fetch(HTTPRequestPtr request, CallbackType callback) {
        fetch(request, request, std::move(callback));
    }

    void fetch(HTTPRequestPtr originalRequest, HTTPRequestPtr request, CallbackType callback);

    template <typename ...Args>
    static std::shared_ptr<HTTPClient> create(Args&& ...args) {
        return std::make_shared<HTTPClient>(std::forward<Args>(args)...);
    }

    const StringMap& getHostnameMapping() const {
        return _hostnameMapping;
    }
protected:
    void onFetchComplete(CallbackType callback, HTTPResponse response) {
        callback(std::move(response));
    }

    IOLoop * _ioloop;
    StringMap _hostnameMapping;
};

typedef std::shared_ptr<HTTPClient> HTTPClientPtr;


class _HTTPConnection: public std::enable_shared_from_this<_HTTPConnection> {
public:
    typedef HTTPClient::CallbackType CallbackType;

    _HTTPConnection(IOLoop *ioloop, HTTPClientPtr client, HTTPRequestPtr originalRequest, HTTPRequestPtr request,
                    CallbackType callback);
    void start();

    template <typename ...Args>
    static std::shared_ptr<_HTTPConnection> create(Args&& ...args) {
        return std::make_shared<_HTTPConnection>(std::forward<Args>(args)...);
    }

    static const StringSet supportedMethods;
protected:
    BaseIOStreamPtr getStream() {
        return _stream.lock();
    }

    void onTimeout();
    void onConnect();
    void onClose();

    Timestamp _startTime;
    IOLoop *_ioloop;
    HTTPClientPtr _client;
    HTTPRequestPtr _originalRequest;
    HTTPRequestPtr _request;
    CallbackType _callback;
    boost::optional<int> _code;
    HTTPHeadersPtr _headers;
    boost::optional<ByteArray> _chunks;
    std::unique_ptr<DecompressObj> _decompressor;
    Timeout _timeout;
    Timeout _connectTimeout;
    BaseIOStreamWPtr _stream;
    BaseIOStreamPtr _streamKeeper;
    std::string _parsedScheme;
    std::string _parsedNetloc;
    std::string _parsedURL;
    std::string _parsedQuery;
    std::string _parsedFragment;
};

typedef std::shared_ptr<_HTTPConnection> _HTTPConnectionPtr;
typedef std::weak_ptr<_HTTPConnection> _HTTPConnectionWPtr;

#endif //TINYCORE_HTTPCLIENT_H
