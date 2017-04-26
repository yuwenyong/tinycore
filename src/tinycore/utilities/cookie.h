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
    static const char *legalCharsPatt;
    static const boost::regex cookiePattern;
};


class TC_COMMON_API Morsel {
public:
    Morsel();

    void setItem(std::string key, std::string value);

    bool isReservedKey(const std::string &key) const {
        return reserved.find(boost::to_lower_copy(key)) != reserved.end();
    }

    const std::string& getValue() const {
        return _value;
    }

    void set(std::string key, std::string val, std::string codedVal,
             const std::vector<char> &legalChars= CookieUtil::legalChars,
             const std::array<char, 256> &idmap= CookieUtil::idmap);

    std::string output(const StringMap *attrs= nullptr, const std::string &header= "Set-Cookie:") const {
        return header + " " + outputString(nullptr);
    }

    std::string outputString(const StringMap *attrs= nullptr) const;

    static const StringMap reserved;
protected:
    StringMap _items;
    std::string _key;
    std::string _value;
    std::string _codedValue;

};


class TC_COMMON_API BaseCookie {
public:
    typedef std::tuple<std::string, std::string> DecodeResultType;
    typedef std::tuple<std::string, std::string> EncodeResultType;
    typedef std::map<std::string, Morsel> MorselContainerType;
    typedef std::function<void (const std::string&, const Morsel&)> CallbackType;

    BaseCookie() = default;

    explicit BaseCookie(const std::string &input) {
        load(input);
    }

    explicit BaseCookie(const StringMap &input) {
        load(input);
    }

    const Morsel& getItem(const std::string &key) const {
        return _items.at(key);
    }

    Morsel& getItem(const std::string &key) {
        return _items.at(key);
    }

    void setItem(const std::string &key, const std::string &value) {
        std::string rval, cval;
        std::tie(rval, cval) = valueEncode(value);
        set(key, std::move(rval), std::move(cval));
    }

    bool contain(const std::string &key) const {
        return _items.find(key) != _items.end();
    }

    void getAll(CallbackType callback) const {
        for (auto &item: _items) {
            callback(item.first, item.second);
        }
    }

    virtual DecodeResultType valueDecode(const std::string &val);
    virtual EncodeResultType valueEncode(const std::string &val);

    std::string output(const StringMap *attrs= nullptr, const std::string &header= "Set-Cookie:",
                       const std::string &sep= "\015\012") const;

    void load(const std::string &rawdata) {
        parseString(rawdata);
    }

    void load(const StringMap &rawdata) {
        for (auto &kv: rawdata) {
            setItem(kv.first, kv.second);
        }
    }
protected:
    void set(const std::string &key, std::string realValue, std::string codedValue) {
        auto iter = _items.find(key);
        if (iter == _items.end()) {
            auto result = _items.emplace(key, Morsel());
            iter = result.first;
        }
        iter->second.set(key, std::move(realValue), std::move(codedValue));
    }

    void parseString(const std::string &str, const boost::regex &patt =CookieUtil::cookiePattern);

    MorselContainerType _items;
};

#endif //TINYCORE_COOKIE_H
