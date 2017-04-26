//
// Created by yuwenyong on 17-3-27.
//

#include "tinycore/utilities/urlparse.h"
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/utilities/string.h"
#include "tinycore/common/errors.h"


const StringSet URLParse::_usesRelative = {"ftp", "http", "gopher", "nntp", "imap",
                                           "wais", "file", "https", "shttp", "mms",
                                           "prospero", "rtsp", "rtspu", "", "sftp",
                                           "svn", "svn+ssh"};

const StringSet URLParse::_usesNetloc = {"ftp", "http", "gopher", "nntp", "telnet",
                                         "imap", "wais", "file", "mms", "https", "shttp",
                                         "snews", "prospero", "rtsp", "rtspu", "rsync", "",
                                         "svn", "svn+ssh", "sftp", "nfs", "git", "git+ssh"};

const StringSet URLParse::_usesParams = {"ftp", "hdl", "prospero", "http", "imap",
                                         "https", "shttp", "rtsp", "rtspu", "sip", "sips",
                                         "mms", "", "sftp", "tel"};

const char * URLParse::_schemeChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.";

const std::map<std::string, char> URLParse::_hexToChar = {
        {"00", '\x00'},  {"01", '\x01'},  {"02", '\x02'},  {"03", '\x03'},
        {"04", '\x04'},  {"05", '\x05'},  {"06", '\x06'},  {"07", '\x07'},
        {"08", '\x08'},  {"09", '\x09'},  {"0A", '\x0a'},  {"0B", '\x0b'},
        {"0C", '\x0c'},  {"0D", '\x0d'},  {"0E", '\x0e'},  {"0F", '\x0f'},
        {"0a", '\x0a'},  {"0b", '\x0b'},  {"0c", '\x0c'},  {"0d", '\x0d'},
        {"0e", '\x0e'},  {"0f", '\x0f'},  {"10", '\x10'},  {"11", '\x11'},
        {"12", '\x12'},  {"13", '\x13'},  {"14", '\x14'},  {"15", '\x15'},
        {"16", '\x16'},  {"17", '\x17'},  {"18", '\x18'},  {"19", '\x19'},
        {"1A", '\x1a'},  {"1B", '\x1b'},  {"1C", '\x1c'},  {"1D", '\x1d'},
        {"1E", '\x1e'},  {"1F", '\x1f'},  {"1a", '\x1a'},  {"1b", '\x1b'},
        {"1c", '\x1c'},  {"1d", '\x1d'},  {"1e", '\x1e'},  {"1f", '\x1f'},
        {"20", '\x20'},  {"21", '\x21'},  {"22", '\x22'},  {"23", '\x23'},
        {"24", '\x24'},  {"25", '\x25'},  {"26", '\x26'},  {"27", '\x27'},
        {"28", '\x28'},  {"29", '\x29'},  {"2A", '\x2a'},  {"2B", '\x2b'},
        {"2C", '\x2c'},  {"2D", '\x2d'},  {"2E", '\x2e'},  {"2F", '\x2f'},
        {"2a", '\x2a'},  {"2b", '\x2b'},  {"2c", '\x2c'},  {"2d", '\x2d'},
        {"2e", '\x2e'},  {"2f", '\x2f'},  {"30", '\x30'},  {"31", '\x31'},
        {"32", '\x32'},  {"33", '\x33'},  {"34", '\x34'},  {"35", '\x35'},
        {"36", '\x36'},  {"37", '\x37'},  {"38", '\x38'},  {"39", '\x39'},
        {"3A", '\x3a'},  {"3B", '\x3b'},  {"3C", '\x3c'},  {"3D", '\x3d'},
        {"3E", '\x3e'},  {"3F", '\x3f'},  {"3a", '\x3a'},  {"3b", '\x3b'},
        {"3c", '\x3c'},  {"3d", '\x3d'},  {"3e", '\x3e'},  {"3f", '\x3f'},
        {"40", '\x40'},  {"41", '\x41'},  {"42", '\x42'},  {"43", '\x43'},
        {"44", '\x44'},  {"45", '\x45'},  {"46", '\x46'},  {"47", '\x47'},
        {"48", '\x48'},  {"49", '\x49'},  {"4A", '\x4a'},  {"4B", '\x4b'},
        {"4C", '\x4c'},  {"4D", '\x4d'},  {"4E", '\x4e'},  {"4F", '\x4f'},
        {"4a", '\x4a'},  {"4b", '\x4b'},  {"4c", '\x4c'},  {"4d", '\x4d'},
        {"4e", '\x4e'},  {"4f", '\x4f'},  {"50", '\x50'},  {"51", '\x51'},
        {"52", '\x52'},  {"53", '\x53'},  {"54", '\x54'},  {"55", '\x55'},
        {"56", '\x56'},  {"57", '\x57'},  {"58", '\x58'},  {"59", '\x59'},
        {"5A", '\x5a'},  {"5B", '\x5b'},  {"5C", '\x5c'},  {"5D", '\x5d'},
        {"5E", '\x5e'},  {"5F", '\x5f'},  {"5a", '\x5a'},  {"5b", '\x5b'},
        {"5c", '\x5c'},  {"5d", '\x5d'},  {"5e", '\x5e'},  {"5f", '\x5f'},
        {"60", '\x60'},  {"61", '\x61'},  {"62", '\x62'},  {"63", '\x63'},
        {"64", '\x64'},  {"65", '\x65'},  {"66", '\x66'},  {"67", '\x67'},
        {"68", '\x68'},  {"69", '\x69'},  {"6A", '\x6a'},  {"6B", '\x6b'},
        {"6C", '\x6c'},  {"6D", '\x6d'},  {"6E", '\x6e'},  {"6F", '\x6f'},
        {"6a", '\x6a'},  {"6b", '\x6b'},  {"6c", '\x6c'},  {"6d", '\x6d'},
        {"6e", '\x6e'},  {"6f", '\x6f'},  {"70", '\x70'},  {"71", '\x71'},
        {"72", '\x72'},  {"73", '\x73'},  {"74", '\x74'},  {"75", '\x75'},
        {"76", '\x76'},  {"77", '\x77'},  {"78", '\x78'},  {"79", '\x79'},
        {"7A", '\x7a'},  {"7B", '\x7b'},  {"7C", '\x7c'},  {"7D", '\x7d'},
        {"7E", '\x7e'},  {"7F", '\x7f'},  {"7a", '\x7a'},  {"7b", '\x7b'},
        {"7c", '\x7c'},  {"7d", '\x7d'},  {"7e", '\x7e'},  {"7f", '\x7f'},
        {"80", '\x80'},  {"81", '\x81'},  {"82", '\x82'},  {"83", '\x83'},
        {"84", '\x84'},  {"85", '\x85'},  {"86", '\x86'},  {"87", '\x87'},
        {"88", '\x88'},  {"89", '\x89'},  {"8A", '\x8a'},  {"8B", '\x8b'},
        {"8C", '\x8c'},  {"8D", '\x8d'},  {"8E", '\x8e'},  {"8F", '\x8f'},
        {"8a", '\x8a'},  {"8b", '\x8b'},  {"8c", '\x8c'},  {"8d", '\x8d'},
        {"8e", '\x8e'},  {"8f", '\x8f'},  {"90", '\x90'},  {"91", '\x91'},
        {"92", '\x92'},  {"93", '\x93'},  {"94", '\x94'},  {"95", '\x95'},
        {"96", '\x96'},  {"97", '\x97'},  {"98", '\x98'},  {"99", '\x99'},
        {"9A", '\x9a'},  {"9B", '\x9b'},  {"9C", '\x9c'},  {"9D", '\x9d'},
        {"9E", '\x9e'},  {"9F", '\x9f'},  {"9a", '\x9a'},  {"9b", '\x9b'},
        {"9c", '\x9c'},  {"9d", '\x9d'},  {"9e", '\x9e'},  {"9f", '\x9f'},
        {"A0", '\xa0'},  {"A1", '\xa1'},  {"A2", '\xa2'},  {"A3", '\xa3'},
        {"A4", '\xa4'},  {"A5", '\xa5'},  {"A6", '\xa6'},  {"A7", '\xa7'},
        {"A8", '\xa8'},  {"A9", '\xa9'},  {"AA", '\xaa'},  {"AB", '\xab'},
        {"AC", '\xac'},  {"AD", '\xad'},  {"AE", '\xae'},  {"AF", '\xaf'},
        {"Aa", '\xaa'},  {"Ab", '\xab'},  {"Ac", '\xac'},  {"Ad", '\xad'},
        {"Ae", '\xae'},  {"Af", '\xaf'},  {"B0", '\xb0'},  {"B1", '\xb1'},
        {"B2", '\xb2'},  {"B3", '\xb3'},  {"B4", '\xb4'},  {"B5", '\xb5'},
        {"B6", '\xb6'},  {"B7", '\xb7'},  {"B8", '\xb8'},  {"B9", '\xb9'},
        {"BA", '\xba'},  {"BB", '\xbb'},  {"BC", '\xbc'},  {"BD", '\xbd'},
        {"BE", '\xbe'},  {"BF", '\xbf'},  {"Ba", '\xba'},  {"Bb", '\xbb'},
        {"Bc", '\xbc'},  {"Bd", '\xbd'},  {"Be", '\xbe'},  {"Bf", '\xbf'},
        {"C0", '\xc0'},  {"C1", '\xc1'},  {"C2", '\xc2'},  {"C3", '\xc3'},
        {"C4", '\xc4'},  {"C5", '\xc5'},  {"C6", '\xc6'},  {"C7", '\xc7'},
        {"C8", '\xc8'},  {"C9", '\xc9'},  {"CA", '\xca'},  {"CB", '\xcb'},
        {"CC", '\xcc'},  {"CD", '\xcd'},  {"CE", '\xce'},  {"CF", '\xcf'},
        {"Ca", '\xca'},  {"Cb", '\xcb'},  {"Cc", '\xcc'},  {"Cd", '\xcd'},
        {"Ce", '\xce'},  {"Cf", '\xcf'},  {"D0", '\xd0'},  {"D1", '\xd1'},
        {"D2", '\xd2'},  {"D3", '\xd3'},  {"D4", '\xd4'},  {"D5", '\xd5'},
        {"D6", '\xd6'},  {"D7", '\xd7'},  {"D8", '\xd8'},  {"D9", '\xd9'},
        {"DA", '\xda'},  {"DB", '\xdb'},  {"DC", '\xdc'},  {"DD", '\xdd'},
        {"DE", '\xde'},  {"DF", '\xdf'},  {"Da", '\xda'},  {"Db", '\xdb'},
        {"Dc", '\xdc'},  {"Dd", '\xdd'},  {"De", '\xde'},  {"Df", '\xdf'},
        {"E0", '\xe0'},  {"E1", '\xe1'},  {"E2", '\xe2'},  {"E3", '\xe3'},
        {"E4", '\xe4'},  {"E5", '\xe5'},  {"E6", '\xe6'},  {"E7", '\xe7'},
        {"E8", '\xe8'},  {"E9", '\xe9'},  {"EA", '\xea'},  {"EB", '\xeb'},
        {"EC", '\xec'},  {"ED", '\xed'},  {"EE", '\xee'},  {"EF", '\xef'},
        {"Ea", '\xea'},  {"Eb", '\xeb'},  {"Ec", '\xec'},  {"Ed", '\xed'},
        {"Ee", '\xee'},  {"Ef", '\xef'},  {"F0", '\xf0'},  {"F1", '\xf1'},
        {"F2", '\xf2'},  {"F3", '\xf3'},  {"F4", '\xf4'},  {"F5", '\xf5'},
        {"F6", '\xf6'},  {"F7", '\xf7'},  {"F8", '\xf8'},  {"F9", '\xf9'},
        {"FA", '\xfa'},  {"FB", '\xfb'},  {"FC", '\xfc'},  {"FD", '\xfd'},
        {"FE", '\xfe'},  {"FF", '\xff'},  {"Fa", '\xfa'},  {"Fb", '\xfb'},
        {"Fc", '\xfc'},  {"Fd", '\xfd'},  {"Fe", '\xfe'},  {"Ff", '\xff'},
        {"a0", '\xa0'},  {"a1", '\xa1'},  {"a2", '\xa2'},  {"a3", '\xa3'},
        {"a4", '\xa4'},  {"a5", '\xa5'},  {"a6", '\xa6'},  {"a7", '\xa7'},
        {"a8", '\xa8'},  {"a9", '\xa9'},  {"aA", '\xaa'},  {"aB", '\xab'},
        {"aC", '\xac'},  {"aD", '\xad'},  {"aE", '\xae'},  {"aF", '\xaf'},
        {"aa", '\xaa'},  {"ab", '\xab'},  {"ac", '\xac'},  {"ad", '\xad'},
        {"ae", '\xae'},  {"af", '\xaf'},  {"b0", '\xb0'},  {"b1", '\xb1'},
        {"b2", '\xb2'},  {"b3", '\xb3'},  {"b4", '\xb4'},  {"b5", '\xb5'},
        {"b6", '\xb6'},  {"b7", '\xb7'},  {"b8", '\xb8'},  {"b9", '\xb9'},
        {"bA", '\xba'},  {"bB", '\xbb'},  {"bC", '\xbc'},  {"bD", '\xbd'},
        {"bE", '\xbe'},  {"bF", '\xbf'},  {"ba", '\xba'},  {"bb", '\xbb'},
        {"bc", '\xbc'},  {"bd", '\xbd'},  {"be", '\xbe'},  {"bf", '\xbf'},
        {"c0", '\xc0'},  {"c1", '\xc1'},  {"c2", '\xc2'},  {"c3", '\xc3'},
        {"c4", '\xc4'},  {"c5", '\xc5'},  {"c6", '\xc6'},  {"c7", '\xc7'},
        {"c8", '\xc8'},  {"c9", '\xc9'},  {"cA", '\xca'},  {"cB", '\xcb'},
        {"cC", '\xcc'},  {"cD", '\xcd'},  {"cE", '\xce'},  {"cF", '\xcf'},
        {"ca", '\xca'},  {"cb", '\xcb'},  {"cc", '\xcc'},  {"cd", '\xcd'},
        {"ce", '\xce'},  {"cf", '\xcf'},  {"d0", '\xd0'},  {"d1", '\xd1'},
        {"d2", '\xd2'},  {"d3", '\xd3'},  {"d4", '\xd4'},  {"d5", '\xd5'},
        {"d6", '\xd6'},  {"d7", '\xd7'},  {"d8", '\xd8'},  {"d9", '\xd9'},
        {"dA", '\xda'},  {"dB", '\xdb'},  {"dC", '\xdc'},  {"dD", '\xdd'},
        {"dE", '\xde'},  {"dF", '\xdf'},  {"da", '\xda'},  {"db", '\xdb'},
        {"dc", '\xdc'},  {"dd", '\xdd'},  {"de", '\xde'},  {"df", '\xdf'},
        {"e0", '\xe0'},  {"e1", '\xe1'},  {"e2", '\xe2'},  {"e3", '\xe3'},
        {"e4", '\xe4'},  {"e5", '\xe5'},  {"e6", '\xe6'},  {"e7", '\xe7'},
        {"e8", '\xe8'},  {"e9", '\xe9'},  {"eA", '\xea'},  {"eB", '\xeb'},
        {"eC", '\xec'},  {"eD", '\xed'},  {"eE", '\xee'},  {"eF", '\xef'},
        {"ea", '\xea'},  {"eb", '\xeb'},  {"ec", '\xec'},  {"ed", '\xed'},
        {"ee", '\xee'},  {"ef", '\xef'},  {"f0", '\xf0'},  {"f1", '\xf1'},
        {"f2", '\xf2'},  {"f3", '\xf3'},  {"f4", '\xf4'},  {"f5", '\xf5'},
        {"f6", '\xf6'},  {"f7", '\xf7'},  {"f8", '\xf8'},  {"f9", '\xf9'},
        {"fA", '\xfa'},  {"fB", '\xfb'},  {"fC", '\xfc'},  {"fD", '\xfd'},
        {"fE", '\xfe'},  {"fF", '\xff'},  {"fa", '\xfa'},  {"fb", '\xfb'},
        {"fc", '\xfc'},  {"fd", '\xfd'},  {"fe", '\xfe'},  {"ff", '\xff'},
};


URLParse::ParseResult URLParse::urlParse(std::string url, std::string scheme, bool allowFragments) {
    std::string netloc, query, fragment, params;
    std::tie(scheme, netloc, url, query, fragment) = urlSplit(std::move(url), std::move(scheme), allowFragments);
    if (_usesParams.find(scheme) != _usesParams.end() && url.find(';') != std::string::npos) {
        std::tie(url, params) = splitParams(url);
    }
    return std::make_tuple(std::move(scheme), std::move(netloc), std::move(url), std::move(params), std::move(query),
                           std::move(fragment));
}


URLParse::SplitResult URLParse::urlSplit(std::string url, std::string scheme, bool allowFragments) {
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
                std::tie(netloc, url) = splitNetloc(url, 2);
                leftBracket = netloc.find('[');
                rightBracket = netloc.find(']');
                if ((leftBracket != std::string::npos && rightBracket == std::string::npos) ||
                    (leftBracket == std::string::npos && rightBracket != std::string::npos)) {
                    ThrowException(ValueError, "Invalid IPv6 URL");
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
        std::tie(netloc, url) = splitNetloc(url, 2);
        leftBracket = netloc.find('[');
        rightBracket = netloc.find(']');
        if ((leftBracket != std::string::npos && rightBracket == std::string::npos) ||
            (leftBracket == std::string::npos && rightBracket != std::string::npos)) {
            ThrowException(ValueError, "Invalid IPv6 URL");
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

std::string URLParse::urlUnparse(const ParseResult &data) {
    std::string scheme, netloc, url, params, query, fragment;
    std::tie(scheme, netloc, url, params, query, fragment) = data;
    if (!params.empty()) {
        url += ';' + params;
    }
    return urlUnsplit(std::make_tuple(scheme, netloc, url, query, fragment));
}

std::string URLParse::urlUnsplit(const SplitResult &data) {
    std::string scheme, netloc, url, query, fragment;
    std::tie(scheme, netloc, url, query, fragment) = data;
    if (!netloc.empty() || (!scheme.empty() && _usesNetloc.find(scheme) != _usesNetloc.end() &&
                            !boost::starts_with(url, "//"))) {
        if (!url.empty() && url[0] != '/') {
            url = '/' + url;
        }
        url = "//" + (netloc.empty() ? std::string("") : netloc) + url;
    }
    if (!scheme.empty()) {
        url = scheme + ':' + url;
    }
    if (!query.empty()) {
        url += '?' + query;
    }
    if (!fragment.empty()) {
        url += '#' + fragment;
    }
    return url;
}

std::string URLParse::urlJoin(const std::string &base, const std::string &url, bool allowFragments) {
    if (base.empty()) {
        return url;
    }
    if (url.empty()) {
        return base;
    }
    std::string bscheme, bnetloc, bpath, bparams, bquery, bfragment;
    std::tie(bscheme, bnetloc, bpath, bparams, bquery, bfragment) = urlParse(base, "", allowFragments);
    std::string scheme, netloc, path, params, query, fragment;
    std::tie(scheme, netloc, path, params, query, fragment) = urlParse(url, bscheme, allowFragments);
    if (scheme != bscheme || _usesRelative.find(scheme) == _usesRelative.end()) {
        return url;
    }
    if (_usesNetloc.find(scheme) != _usesNetloc.end()) {
        if (!netloc.empty()) {
            return urlUnparse(std::make_tuple(scheme, netloc, path, params, query, fragment));
        }
        netloc = std::move(bnetloc);
    }
    if (!path.empty() && path[0] == '/') {
        return urlUnparse(std::make_tuple(scheme, netloc, path, params, query, fragment));
    }
    if (path.empty() && params.empty()) {
        path = std::move(bpath);
        params = std::move(params);
        if (query.empty()) {
            query = std::move(bquery);
        }
        return urlUnparse(std::make_tuple(scheme, netloc, path, params, query, fragment));
    }
    StringVector bpathSplit, pathSplit, segments;
    bpathSplit = String::split(bpath, '/');
    pathSplit = String::split(path, '/');
    segments = std::move(bpathSplit);
    segments.pop_back();
    segments.insert(segments.end(), pathSplit.begin(), pathSplit.end());
    if (segments.back() == ".") {
        segments.back().clear();
    }
    segments.erase(std::remove(segments.begin(), segments.end(), "."), segments.end());
    size_t i, n;
    bool done;
    while (true) {
        i = 1;
        n = segments.size() - 1;
        done = true;
        while (i < n) {
            if (segments[i] == ".." && segments[i - 1] != "" && segments[i - 1] != "..") {
                auto iter = std::next(segments.begin(), i - 1);
                segments.erase(iter, std::next(iter, 2));
                done = false;
                break;
            }
            ++i;
        }
        if (done) {
            break;
        }
    }
    if (segments.size() == 2 && segments[0] == "" && segments[1] == "..") {
        segments.back().clear();
    } else if (segments.size() > 2 && segments.back() == "..") {
        segments.pop_back();
        segments.back() = "";
    }
    return urlUnparse(std::make_tuple(scheme, netloc, boost::join(segments, "/"), params, query, fragment));
}

std::tuple<std::string, std::string> URLParse::urlDefrag(const std::string &url) {
    if (url.find('#') != std::string::npos) {
        std::string s, n, p, a, q, frag;
        std::tie(s, n, p, a, q, frag) = urlParse(url);
        std::string defrag = urlUnparse(std::make_tuple(s, n, p, a, q, ""));
        return std::make_tuple(defrag, frag);
    } else {
        return std::make_tuple(url, "");
    }
}

std::string URLParse::unquote(const std::string &s) {
    std::vector<std::string> bits;
    bits = String::split(s, '%');
    if (bits.size() == 1) {
        return s;
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
            res.emplace_back(1, trans->second);
            res.emplace_back(iter->substr(2));
        }
    }
    return boost::join(res, "");
}

std::string URLParse::unquotePlus(const std::string &s) {
    return unquote(boost::replace_all_copy(s, "+", " "));
}

URLParse::QueryArguments URLParse::parseQS(const std::string &queryString, bool keepBlankValues, bool strictParsing) {
    QueryArguments dict;
    decltype(dict.begin()) iter;
    QueryArgumentsList arguments = parseQSL(queryString, keepBlankValues, strictParsing);
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


URLParse::QueryArgumentsList URLParse::parseQSL(const std::string &queryString, bool keepBlankValues, bool strictParsing) {
    QueryArgumentsList r;
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
                ThrowException(ValueError, std::move(error));
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

std::tuple<std::string, std::string> URLParse::splitParams(const std::string &url) {
    size_t i;
    i = url.rfind('/');
    if (i != std::string::npos) {
        i = url.find(';', i);
        if (i == std::string::npos) {
            return std::make_tuple(url, "");
        }
    } else {
        i = url.find(';');
    }
    return std::make_tuple(url.substr(0, i), url.substr(i + 1));
}

std::tuple<std::string, std::string> URLParse::splitNetloc(const std::string &url, size_t start) {
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