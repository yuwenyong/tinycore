//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_URLPARSE_H
#define TINYCORE_URLPARSE_H

#include "tinycore/common/common.h"
#include <tuple>


class TC_COMMON_API URLParse {
public:
    typedef std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> ParseResult;
    typedef std::tuple<std::string, std::string, std::string, std::string, std::string> SplitResult;
    typedef std::map<std::string, StringVector> QueryArguments;
    typedef std::vector<std::pair<std::string, std::string>> QueryArgumentsList;

    static ParseResult urlParse(const std::string &url, const std::string &scheme="", bool allowFragments=true);
    static SplitResult urlSplit(const std::string &url, const std::string &scheme="", bool allowFragments=true);
    static std::string urlUnparse(const ParseResult &data);
    static std::string urlUnsplit(const SplitResult &data);
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

//class URLSplitKey {
//public:
//    URLSplitKey(std::string url,
//                std::string scheme,
//                bool allowFragments)
//            : _url(std::move(url)),
//              _scheme(std::move(scheme)),
//              _allowFragments(allowFragments) {
//
//    }
//
//    URLSplitKey(URLSplitKey &&rhs) noexcept
//            : _url(std::move(rhs._url)),
//              _scheme(std::move(rhs._scheme)),
//              _allowFragments(rhs._allowFragments) {
//
//    }
//
//    URLSplitKey &operator=(URLSplitKey &&rhs) noexcept {
//        if (this != &rhs) {
//            _url = std::move(rhs._url);
//            _scheme = std::move(rhs._scheme);
//            _allowFragments = rhs._allowFragments;
//        }
//        return *this;
//    }
//
//    bool operator<(const URLSplitKey &rhs) const {
//        if (_url != rhs._url) {
//            return _url < rhs._url;
//        }
//        if (_scheme != rhs._scheme) {
//            return _scheme < rhs._scheme;
//        }
//        if (_allowFragments != rhs._allowFragments) {
//            return !_allowFragments;
//        }
//        return false;
//    }
//
//protected:
//    std::string _url;
//    std::string _scheme;
//    bool _allowFragments;
//};

#endif //TINYCORE_URLPARSE_H
