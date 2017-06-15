//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_URLPARSE_H
#define TINYCORE_URLPARSE_H

#include "tinycore/common/common.h"
#include <tuple>
#include <boost/optional.hpp>


class TC_COMMON_API URLSplitResult {
public:
    URLSplitResult(std::string scheme,
                   std::string netloc,
                   std::string path,
                   std::string query,
                   std::string fragment)
            : _scheme(std::move(scheme))
            , _netloc(std::move(netloc))
            , _path(std::move(path))
            , _query(std::move(query))
            , _fragment(std::move(fragment)) {

    }

    const std::string& getScheme() const {
        return _scheme;
    }

    const std::string& getNetloc() const {
        return _netloc;
    }

    const std::string& getPath() const {
        return _path;
    }

    const std::string& getQuery() const {
        return _query;
    }

    const std::string& getFragment() const {
        return _fragment;
    }

    const std::string* getHostName() const {
        if (!_parsed) {
            parse();
        }
        return _hostName.get_ptr();
    }

    const unsigned short* getPort() const {
        if (!_parsed) {
            parse();
        }
        return _port.get_ptr();
    }

    const std::string* getUserName() const {
        if (!_parsed) {
            parse();
        }
        return _userName.get_ptr();
    }

    const std::string& getPassword() const {
        if (!_parsed) {
            parse();
        }
        return _password;
    }
protected:
    void parse() const;

    std::string _scheme;
    std::string _netloc;
    std::string _path;
    std::string _query;
    std::string _fragment;
    mutable bool _parsed{false};
    mutable boost::optional<std::string> _hostName;
    mutable boost::optional<unsigned short> _port;
    mutable boost::optional<std::string> _userName;
    mutable std::string _password;
};


class TC_COMMON_API URLParseResult: public URLSplitResult {
public:
    URLParseResult(std::string scheme,
                   std::string netloc,
                   std::string path,
                   std::string params,
                   std::string query,
                   std::string fragment)
            : URLSplitResult(std::move(scheme),
                             std::move(netloc),
                             std::move(path),
                             std::move(query),
                             std::move(fragment))
            , _params(std::move(params)) {

    }

    const std::string& getParams() const {
        return _params;
    }
protected:
    std::string _params;
};


class TC_COMMON_API URLParse {
public:
    typedef std::tuple<std::string, std::string, std::string, std::string, std::string> SplitResult;
    typedef std::map<std::string, StringVector> QueryArguments;
    typedef std::vector<std::pair<std::string, std::string>> QueryArgumentsList;

    static URLParseResult urlParse(const std::string &url, const std::string &scheme="", bool allowFragments=true);
    static URLSplitResult urlSplit(const std::string &url, const std::string &scheme="", bool allowFragments=true);
    static std::string urlUnparse(const URLParseResult &data);
    static std::string urlUnsplit(const URLSplitResult &data);
    static std::string urlJoin(const std::string &base, const std::string &url, bool allowFragments=true);
    static std::tuple<std::string, std::string> urlDefrag(const std::string &url);
    static std::string unquote(const std::string &s);
    static std::string unquotePlus(const std::string &s);
    static std::string quote(const std::string &s, std::string safe="/");
    static std::string quotePlus(const std::string &s, const std::string safe="");
    static std::string urlEncode(const StringMap &query);
    static std::string urlEncode(const QueryArgumentsList &query);
    static std::string urlEncode(const QueryArguments &query);
    static QueryArguments parseQS(const std::string &queryString, bool keepBlankValues=false,
                                    bool strictParsing=false);
    static QueryArgumentsList parseQSL(const std::string &queryString, bool keepBlankValues=false,
                                         bool strictParsing=false);
protected:
    static std::tuple<std::string, std::string> splitParams(const std::string &url);
    static std::tuple<std::string, std::string> splitNetloc(const std::string &url, size_t start=0);

    static const StringSet _usesRelative;
    static const StringSet _usesNetloc;
    static const StringSet _usesParams;
    static const char * _schemeChars;
    static const std::map<std::string, char> _hexToChar;
    static const char * _alwaysSafe;
    static const std::map<char, std::string> _safeMap;
};


#endif //TINYCORE_URLPARSE_H
