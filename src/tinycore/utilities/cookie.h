//
// Created by yuwenyong on 17-4-21.
//

#ifndef TINYCORE_COOKIE_H
#define TINYCORE_COOKIE_H

#include "tinycore/common/common.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "tinycore/common/errors.h"


DECLARE_EXCEPTION(CookieError, Exception);


class TC_COMMON_API CookieUtil {
public:
    static std::string quote(const std::string &s, const std::vector<char> &legalChars= CookieUtil::legalChars,
                             const std::array<char, 256> &idmap= CookieUtil::idmap);
    static std::string unquote(const std::string &s);

    static const std::vector<char> legalChars;
    static const std::array<char, 256> idmap;
    static const std::map<char, std::string> translator;
    static const boost::regex octalPatt;
    static const boost::regex quotePatt;
};


class TC_COMMON_API Morsel {
public:
    Morsel();

    std::string& operator[](std::string key);

    bool isReservedKey(std::string key) const {
        boost::to_lower(key);
        return reserved.find(key) != reserved.end();
    }

    void set(std::string key, std::string val, std::string codedVal,
             const std::vector<char> &legalChars= CookieUtil::legalChars,
             const std::array<char, 256> &idmap= CookieUtil::idmap);

    std::string output(const StringMap &attrs= reserved, std::string header= "Set-Cookie:") {
        return header + " " + outputString(attrs);
    }

    std::string outputString(const StringMap &attrs= reserved) const;

    static const StringMap reserved;
protected:
    StringMap _items;
    std::string _key;
    std::string _value;
    std::string _codedValue;

};


class TC_COMMON_API BaseCookie {
public:

};

#endif //TINYCORE_COOKIE_H
