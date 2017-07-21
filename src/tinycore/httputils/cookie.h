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

    std::string& operator[](const std::string &key);

    bool isReservedKey(const std::string &key) const {
        return reserved.find(boost::to_lower_copy(key)) != reserved.end();
    }

    const std::string& getValue() const {
        return _value;
    }

    void set(const std::string &key, const std::string &val, const std::string &codedVal,
             const std::vector<char> &legalChars= CookieUtil::legalChars,
             const std::array<char, 256> &idmap= CookieUtil::idmap);

    std::string output(const StringMap *attrs= nullptr, const std::string &header= "Set-Cookie:") const {
        return header + " " + outputString(attrs);
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

    class CookieSetter {
    public:
        CookieSetter(BaseCookie *cookie, const std::string *key, Morsel *morsel)
                : _cookie(cookie)
                , _key(key)
                , _morsel(morsel) {

        }

        CookieSetter& operator=(const std::string &value) {
            std::string rval, cval;
            std::tie(rval, cval) = _cookie->valueEncode(value);
            _morsel->set(*_key, rval, cval);
            return *this;
        }
    protected:
        BaseCookie *_cookie;
        const std::string *_key;
        Morsel *_morsel;
    };

    BaseCookie() = default;

    explicit BaseCookie(const std::string &input) {
        load(input);
    }

    explicit BaseCookie(const StringMap &input) {
        load(input);
    }

    virtual ~BaseCookie();

    const Morsel& at(const std::string &key) const {
        return _items.at(key);
    }

    Morsel& at(const std::string &key) {
        return _items.at(key);
    }

    CookieSetter operator[](const std::string &key) {
        auto iter = _items.find(key);
        if (iter == _items.end()) {
            auto result = _items.emplace(key, Morsel());
            iter = result.first;
        }
        return CookieSetter(this, &(iter->first), &(iter->second));
    }

    bool has(const std::string &key) const {
        return _items.find(key) != _items.end();
    }

    void erase(const std::string &key) {
        _items.erase(key);
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
            (*this)[kv.first] = kv.second;
        }
    }

    void clear() {
        _items.clear();
    }
protected:
    void set(const std::string &key, const std::string &realValue, const std::string &codedValue) {
        auto iter = _items.find(key);
        if (iter == _items.end()) {
            auto result = _items.emplace(key, Morsel());
            iter = result.first;
        }
        iter->second.set(key, realValue, codedValue);
    }

    void parseString(const std::string &str, const boost::regex &patt =CookieUtil::cookiePattern);

    MorselContainerType _items;
};


class SimpleCookie: public BaseCookie {
public:
    using BaseCookie::BaseCookie;

    SimpleCookie() = default;
    DecodeResultType valueDecode(const std::string &val) override;
    EncodeResultType valueEncode(const std::string &val) override;
};

#endif //TINYCORE_COOKIE_H
