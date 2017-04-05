//
// Created by yuwenyong on 17-3-27.
//

#ifndef TINYCORE_URLPARSE_H
#define TINYCORE_URLPARSE_H

#include "tinycore/common/common.h"
#include <tuple>
#include <mutex>


typedef std::tuple<std::string, std::string, std::string, std::string, std::string> URLSplitResult;
typedef std::map<std::string, std::vector<std::string>> RequestArguments;
typedef std::vector<std::pair<std::string, std::string>> RequestArgumentsList;


class TC_COMMON_API URLParse {
public:
    static URLSplitResult urlSplit(std::string url, std::string scheme={}, bool allowFragments=true);
    static std::string unquote(const std::string &s);
    static RequestArguments parseQS(const std::string &queryString, bool keepBlankValues=false,
                                    bool strictParsing=false);
    static RequestArgumentsList parseQSL(const std::string &queryString, bool keepBlankValues=false,
                                         bool strictParsing=false);
protected:
    static std::tuple<std::string, std::string> splitNetloc(const std::string &url, size_t start=0);

    static const char * _schemeChars;
    static bool _hexToCharInited;
    static std::mutex _hexToCharLock;
    static std::map<std::string, char> _hexToChar;
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
