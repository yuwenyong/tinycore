//
// Created by yuwenyong on 17-6-15.
//

#ifndef TINYCORE_PARAMS_H
#define TINYCORE_PARAMS_H

#ifdef BOOST_PARAMETER_MAX_ARITY
#undef BOOST_PARAMETER_MAX_ARITY
#endif
#define BOOST_PARAMETER_MAX_ARITY 25
#include <boost/parameter.hpp>


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
BOOST_PARAMETER_NAME(streamingCallback)
BOOST_PARAMETER_NAME(headerCallback)
//BOOST_PARAMETER_NAME(prepareCurlCallback)
BOOST_PARAMETER_NAME(proxyHost)
BOOST_PARAMETER_NAME(proxyPort)
BOOST_PARAMETER_NAME(proxyUserName)
BOOST_PARAMETER_NAME(proxyPassword)
BOOST_PARAMETER_NAME(allowNonstandardMethods)
BOOST_PARAMETER_NAME(validateCert)
BOOST_PARAMETER_NAME(caCerts)
BOOST_PARAMETER_NAME(clientKey)
BOOST_PARAMETER_NAME(clientCert)
}


#define ARG_url opts::_url
#define ARG_method opts::_method
#define ARG_headers opts::_headers
#define ARG_body opts::_body
#define ARG_authUserName opts::_authUserName
#define ARG_authPassword opts::_authPassword
#define ARG_connectTimeout opts::_connectTimeout
#define ARG_requestTimeout opts::_requestTimeout
#define ARG_ifModifiedSince opts::_ifModifiedSince
#define ARG_followRedirects opts::_followRedirects
#define ARG_maxRedirects opts::_maxRedirects
#define ARG_userAgent opts::_userAgent
#define ARG_useGzip opts::_useGzip
#define ARG_networkInterface opts::_networkInterface
#define ARG_streamingCallback opts::_streamingCallback
#define ARG_headerCallback opts::_headerCallback
#define ARG_proxyHost opts::_proxyHost
#define ARG_proxyPort opts::_proxyPort
#define ARG_proxyUserName opts::_proxyUserName
#define ARG_proxyPassword opts::_proxyPassword
#define ARG_allowNonstandardMethods opts::_allowNonstandardMethods
#define ARG_validateCert opts::_validateCert
#define ARG_caCerts opts::_caCerts
#define ARG_clientKey opts::_clientKey
#define ARG_clientCert opts::_clientCert


#endif //TINYCORE_PARAMS_H
