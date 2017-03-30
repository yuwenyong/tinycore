//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/utilities/urlparse.h"
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/utilities/string.h"


const char * URLParse::_schemeChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.";
bool URLParse::_hexToCharInited = false;
std::mutex URLParse::_hexToCharLock;
std::map<std::string, char> URLParse::_hexToChar;


URLSplitResult URLParse::urlSplit(std::string url, std::string scheme, bool allowFragments) {
    std::string netloc, query, fragment;
    size_t leftBracket, rightBracket, pos;
    auto i = url.find(':');
    if (i != std::string::npos) {
        std::string temp = url.substr(0, i);
        if (boost::ilexicographical_compare(temp, "http")) {
            boost::to_lower(temp);
            scheme = std::move(temp);
            url = url.substr(i + 1);
            if (boost::starts_with(url, "//")) {
                std::tie(netloc, url) = _splitNetloc(url, 2);
                leftBracket = netloc.find('[');
                rightBracket = netloc.find(']');
                if ((leftBracket != std::string::npos && rightBracket == std::string::npos) ||
                    (leftBracket == std::string::npos && rightBracket != std::string::npos)) {
                    throw ValueError("Invalid IPv6 URL");
                }
            }
            if (allowFragments) {
                pos = url.find('#');
                if (pos != std::string::npos) {
                    fragment = url.substr(pos + 1);
                    url = url.substr(0, pos);
                }
            }
            pos = url.find('?');
            if (pos != std::string::npos) {
                query = url.substr(pos + 1);
                url = url.substr(0, pos);
            }
            return std::make_tuple(std::move(scheme), std::move(netloc), std::move(url), std::move(query),
                                   std::move(fragment));
        }
        pos = temp.find_first_not_of(_schemeChars);
        if (pos == std::string::npos) {
            std::string rest = url.substr(i + 1);
            if (rest.empty() || rest.find_first_not_of("0123456789") != std::string::npos) {
                boost::to_lower(temp);
                scheme = std::move(temp);
                url = std::move(rest);
            }
        }
    }
    if (boost::starts_with(url, "//")) {
        std::tie(netloc, url) = _splitNetloc(url, 2);
        leftBracket = netloc.find('[');
        rightBracket = netloc.find(']');
        if ((leftBracket != std::string::npos && rightBracket == std::string::npos) ||
            (leftBracket == std::string::npos && rightBracket != std::string::npos)) {
            throw ValueError("Invalid IPv6 URL");
        }
    }
    if (allowFragments) {
        pos = url.find('#');
        if (pos != std::string::npos) {
            fragment = url.substr(pos + 1);
            url = url.substr(0, pos);
        }
    }
    pos = url.find('?');
    if (pos != std::string::npos) {
        query = url.substr(pos + 1);
        url = url.substr(0, pos);
    }
    return std::make_tuple(std::move(scheme), std::move(netloc), std::move(url), std::move(query),
                           std::move(fragment));
}


std::string URLParse::unquote(const std::string &s) {
    std::vector<std::string> bits;
    bits = String::split(s, '%');
    if (bits.size() == 1) {
        return s;
    }
    if (!_hexToCharInited) {
        std::lock_guard<std::mutex> lock(_hexToCharLock);
        if (!_hexToCharInited) {
            const std::string hexDig("0123456789ABCDEFabcdef");
            std::string key;
            char value;
            for (auto a = hexDig.cbegin(); a != hexDig.cend(); ++ a) {
                for (auto b = hexDig.cbegin(); b != hexDig.cend(); ++b) {
                    key = *a;
                    key += *b;
                    value = (char)std::stoi(key, nullptr, 16);
                    _hexToChar.emplace(std::move(key), value);
                }
            }
            _hexToCharInited = true;
        }
    }
    std::vector<std::string> res;
    std::string item;
    decltype(_hexToChar.cbegin()) trans;
    for (auto iter = bits.begin(); iter != bits.end(); ++iter) {
        if (res.empty()) {
            res.emplace_back(std::move(*iter));
            continue;
        }
        if (iter->size() < 2) {
            res.emplace_back("%");
            res.emplace_back(std::move(*iter));
            continue;
        }
        item = iter->substr(0, 2);
        trans = _hexToChar.find(item);
        if (trans == _hexToChar.end()) {
            res.emplace_back("%");
            res.emplace_back(std::move(*iter));
        } else {
            res.emplace_back(&(trans->second), 1);
            res.emplace_back(iter->substr(2));
        }
    }
    return boost::join(res, "");
}


RequestArguments URLParse::parseQS(const std::string &queryString, bool keepBlankValues, bool strictParsing) {
    RequestArguments dict;
    decltype(dict.begin()) iter;
    RequestArgumentsList arguments = parseQSL(queryString, keepBlankValues, strictParsing);
    for (auto &nv: arguments) {
        if (nv.second.empty()) {
            continue;
        }
        iter = dict.find(nv.first);
        if (iter != dict.end()) {
            iter->second.emplace_back(std::move(nv.second));
        } else {
            dict.emplace(std::move(nv.first), std::vector<std::string>{std::move(nv.second)});
        }
    }
    return dict;
}


RequestArgumentsList URLParse::parseQSL(const std::string &queryString, bool keepBlankValues, bool strictParsing) {
    RequestArgumentsList r;
    StringVector s1, s2, pairs;
    s1 = String::split(queryString, '&');
    for (auto &v1: s1) {
        s2 = String::split(v1, ';');
        for (auto &v2: s2) {
            pairs.push_back(std::move(v2));
        }
    }
    size_t pos;
    std::string name, value;
    for (auto &nameValue: pairs) {
        if (nameValue.empty() && !strictParsing) {
            continue;
        }
        pos = nameValue.find('=');
        if (pos == std::string::npos) {
            if (strictParsing) {
                std::string error;
                error = "bad query field:" + nameValue;
                throw ValueError(error);
            }
            if (keepBlankValues) {
                name = nameValue;
                value = "";
            } else {
                continue;
            }
        } else {
            name = nameValue.substr(0, pos);
            value = nameValue.substr(pos + 1);
        }
        if (keepBlankValues || !value.empty()) {
            boost::replace_all(name, "+", " ");
            name = unquote(name);
            boost::replace_all(value, "+", " ");
            value = unquote(value);
            r.push_back(std::make_pair(std::move(name), std::move(value)));
        }
    }
    return r;
}


std::tuple<std::string, std::string> URLParse::_splitNetloc(const std::string &url, size_t start) {
    size_t delim = url.size(), wdelim;
    char delimiters[3] = {'/', '?', '#'};
    for (char delimiter: delimiters) {
        wdelim = url.find(delimiter, start);
        if (wdelim != std::string::npos) {
            delim = std::min(delim, wdelim);
        }
    }
    return std::make_tuple(url.substr(start, delim - start), url.substr(delim));
}