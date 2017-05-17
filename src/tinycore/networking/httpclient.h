//
// Created by yuwenyong on 17-5-17.
//

#ifndef TINYCORE_HTTPCLIENT_H
#define TINYCORE_HTTPCLIENT_H

#include "tinycore/common/common.h"
#include <boost/parameter.hpp>
#include <boost/optional.hpp>
#include "tinycore/networking/httputil.h"
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


class HTTPRequestImpl {
public:
    typedef std::function<void (const ByteArray &)> StreamCallbackType;
    typedef std::function<void (const std::string &)> HeaderCallbackType;
    typedef std::chrono::steady_clock ClockType;
    typedef ClockType::time_point TimePointType;

    template <typename ArgumentPack>
    HTTPRequestImpl(ArgumentPack const& args) {
        _headers = args[opts::_headers];
        boost::optional<DateTime> ifModifiedSince = args[opts::_ifModifiedSince | boost::none];
        if (ifModifiedSince) {
            std::string timestamp = String::formatUTCDate(ifModifiedSince.get(), true);
            _headers.setItem("If-Modified-Since", timestamp);
        }
        if (!_headers.contain("Pragma")) {
            _headers.setItem("Pragma", "");
        }
        _proxyHost = args[opts::_proxyHost | boost::none];
        _proxyPort = args[opts::_proxyPort | boost::none];
        _proxyUserName = args[opts::_proxyUserName | boost::none];
        _proxyPassword = args[opts::_proxyPassword];
        if (!_headers.contain("Expect")) {
            _headers.setItem("Expect", "");
        }
        _url = args[opts::_url];
        _method = args[opts::_method];
        _body = args[opts::_body | boost::none];
        _authUserName = args[opts::_authUserName | boost::none];
        _authPassword = args[opts::_authPassword | boost::none];
        _connectTimeout = args[opts::_connectTimeout];
        _requestTimeout = args[opts::_requestTimeout];
        _followRedirects = args[opts::_followRedirects];
        _maxRedirects = args[opts::_maxRedirects];
        _userAgent = args[opts::_userAgent | boost::none];
        _useGzip = args[opts::_useGzip];
        _networkInterface = args[opts::_networkInterface | boost::none];
        _streamCallback = args[opts::_streamCallback];
        _headerCallback = args[opts::_headerCallback];
        _allowStandardMethods = args[opts::_allowNonstandardMethods];
        _validateCert = args[opts::_validateCert];
        _caCerts = args[opts::_caCerts | boost::none];
        _startTime = ClockType::now();
    }

protected:
    std::string _url;
    std::string _method;
    HTTPHeaders _headers;
    boost::optional<ByteArray> _body;
    boost::optional<std::string> _authUserName;
    boost::optional<std::string> _authPassword;
    float _connectTimeout;
    float _requestTimeout;
    bool _followRedirects;
    int _maxRedirects;
    boost::optional<std::string> _userAgent;
    bool _useGzip;
    boost::optional<std::string> _networkInterface;
    StreamCallbackType _streamCallback;
    HeaderCallbackType _headerCallback;
    boost::optional<std::string> _proxyHost;
    boost::optional<unsigned short> _proxyPort;
    boost::optional<std::string> _proxyUserName;
    std::string _proxyPassword;
    bool _allowStandardMethods;
    bool _validateCert;
    boost::optional<std::string> _caCerts;
    TimePointType _startTime;
};


class HTTPRequest: public HTTPRequestImpl {
public:
    BOOST_PARAMETER_CONSTRUCTOR(HTTPRequest, (HTTPRequestImpl), opts::tag, (
            required
                    (url, (std::string))
    )(
            optional
                    (method, (std::string), "GET")
                    (headers, (HTTPHeaders), HTTPHeaders())
                    (body, (ByteArray), ByteArray())
                    (authUserName, (std::string))
                    (authPassword, (std::string))
                    (connectTimeout, (float), 20.0f)
                    (requestTimeout, (float), 20.0f)
                    (ifModifiedSince, (DateTime))
                    (followRedirects, (bool), true)
                    (maxRedirects, (int), 5)
                    (userAgent, (std::string))
                    (useGzip, (bool), true)
                    (networkInterface, (std::string))
                    (streamCallback, (StreamCallbackType), nullptr)
                    (headerCallback, (HeaderCallbackType), nullptr)
                    (proxyHost, (std::string))
                    (proxyPort, (unsigned short))
                    (proxyUserName, (std::string))
                    (proxyPassword, (std::string), "")
                    (allowStandardMethods, (bool), false)
                    (validateCert, (bool), true)
                    (ca_certs, (std::string))
    ))
};

#endif //TINYCORE_HTTPCLIENT_H
