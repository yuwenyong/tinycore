//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_URLPARSE_H
#define TINYCORE_URLPARSE_H

#include "tinycore/common/common.h"
#include <tuple>
#include <boost/optional.hpp>


using URLSplitResultBase = std::tuple<std::string, std::string, std::string, std::string, std::string>;


class TC_COMMON_API URLSplitResult: public URLSplitResultBase {
public:
    URLSplitResult(std::string scheme,
                   std::string netloc,
                   std::string path,
                   std::string query,
                   std::string fragment)
            : URLSplitResultBase(std::move(scheme),
                                 std::move(netloc),
                                 std::move(path),
                                 std::move(query),
                                 std::move(fragment)) {

    }

    const std::string& getScheme() const {
        return std::get<0>(*this);
    }

    const std::string& getNetloc() const {
        return std::get<1>(*this);
    }

    const std::string& getPath() const {
        return std::get<2>(*this);
    }

    const std::string& getQuery() const {
        return std::get<3>(*this);
    }

    const std::string& getFragment() const {
        return std::get<4>(*this);
    }

    boost::optional<std::string> getUserName() const;
    boost::optional<std::string> getPassword() const;
    boost::optional<std::string> getHostName() const;
    boost::optional<unsigned short> getPort() const;
    std::string getURL() const;
};


using URLParseResultBase = std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>;


class TC_COMMON_API URLParseResult : public URLParseResultBase {
public:
    URLParseResult(std::string scheme,
                   std::string netloc,
                   std::string path,
                   std::string params,
                   std::string query,
                   std::string fragment)
            : URLParseResultBase(std::move(scheme),
                                 std::move(netloc),
                                 std::move(path),
                                 std::move(params),
                                 std::move(query),
                                 std::move(fragment)) {

    }

    const std::string& getScheme() const {
        return std::get<0>(*this);
    }

    const std::string& getNetloc() const {
        return std::get<1>(*this);
    }

    const std::string& getPath() const {
        return std::get<2>(*this);
    }

    const std::string& getParams() const {
        return std::get<3>(*this);
    }

    const std::string& getQuery() const {
        return std::get<4>(*this);
    }

    const std::string& getFragment() const {
        return std::get<5>(*this);
    }

    boost::optional<std::string> getUserName() const;
    boost::optional<std::string> getPassword() const;
    boost::optional<std::string> getHostName() const;
    boost::optional<unsigned short> getPort() const;
    std::string getURL() const;
};


using QueryArgMap = std::map<std::string, std::string>;
using QueryArgMultiMap = std::multimap<std::string, std::string>;
using QueryArgList = std::vector<std::pair<std::string, std::string>>;
using QueryArgListMap = std::map<std::string, std::vector<std::string>>;


class TC_COMMON_API URLParse {
public:
    static URLParseResult urlParse(std::string url, std::string cheme="", bool allowFragments=true);
    static URLSplitResult urlSplit(std::string url, std::string scheme="", bool allowFragments=true);
    static std::string urlUnparse(const URLParseResult &data);
    static std::string urlUnsplit(const URLSplitResult &data);
    static std::string urlJoin(const std::string &base, const std::string &url, bool allowFragments=true);
    static std::tuple<std::string, std::string> urlDefrag(const std::string &url);
    static std::string unquote(const std::string &s);
    static std::string unquotePlus(const std::string &s);
    static std::string quote(const std::string &s, std::string safe="/");
    static std::string quotePlus(const std::string &s, const std::string &safe="");
    static std::string urlEncode(const QueryArgMap &query);
    static std::string urlEncode(const QueryArgMultiMap &query);
    static std::string urlEncode(const QueryArgList &query);
    static std::string urlEncode(const QueryArgListMap &query);
    static QueryArgListMap parseQS(const std::string &queryString, bool keepBlankValues=false,
                                    bool strictParsing=false);
    static QueryArgList parseQSL(const std::string &queryString, bool keepBlankValues=false, bool strictParsing=false);
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
