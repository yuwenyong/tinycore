//
// Created by yuwenyong on 17-9-6.
//

#include "tinycore/utilities/strutil.h"


void StrUtil::capitalizeInplace(std::string &s) {
    size_t i, len = s.length();
    if (len > 0) {
        s[0] = (char)std::toupper(s[0]);
    }
    for (i = 1; i < len; ++i) {
        s[i] = (char)std::tolower(s[i]);
    }
}

size_t StrUtil::count(const std::string &s, char c, size_t start, size_t len) {
    size_t e = len ? std::min(start + len, s.size()) : s.size();
    if (start >= e) {
        return 0;
    }
    size_t i = start, r = 0;
    while ((i = s.find(c, i)) != std::string::npos) {
        if (i + 1 > e) {
            break;
        }
        ++i;
        ++r;
    }
    return r;
}

size_t StrUtil::count(const std::string &s, const std::string &sub, size_t start, size_t len) {
    size_t e = len ? std::min(start + len, s.size()) : s.size();
    if (start >= e) {
        return 0;
    }
    size_t i = start, r = 0, m = sub.size();
    while ((i = s.find(sub, i)) != std::string::npos) {
        if (i + m > e) {
            break;
        }
        i += m;
        ++r;
    }
    return r;
}

size_t StrUtil::count(const std::string &s, std::function<bool(char)> pred) {
    size_t r = 0;
    for (char c: s) {
        if (pred(c)) {
            ++r;
        }
    }
    return r;
}

PartitionResult StrUtil::partition(const std::string &s, const std::string &sep) {
    auto pos = s.find(sep);
    if (pos == std::string::npos) {
        return std::make_tuple(s, "", "");
    } else {
        std::string before, after;
        before.assign(s.begin(), std::next(s.begin(), pos));
        after.assign(std::next(s.begin(), pos + sep.size()), s.end());
        return std::make_tuple(before, sep, after);
    }
}

PartitionResult StrUtil::rpartition(const std::string &s, const std::string &sep) {
    auto pos = s.find(sep);
    if (pos == std::string::npos) {
        return std::make_tuple("", "", s);
    } else {
        std::string before, after;
        before.assign(s.begin(), std::next(s.begin(), pos));
        after.assign(std::next(s.begin(), pos + sep.size()), s.end());
        return std::make_tuple(before, sep, after);
    }
}

StringVector StrUtil::split(const std::string &s, const std::string &delim, bool keepEmpty) {
    StringVector result;
    if (delim.empty()) {
        result.push_back(s);
        return result;
    }
    std::string::const_iterator beg = s.begin(), end;
    while (true) {
        end = std::search(beg, s.end(), delim.begin(), delim.end());
        std::string temp(beg, end);
        if (keepEmpty || !temp.empty()) {
            result.push_back(temp);
        }
        if (end == s.end()) {
            break;
        }
        beg = end + delim.size();
    }
    return result;
}

StringVector StrUtil::splitLines(const std::string &s, bool keepends) {
    StringVector result;
    size_t i = 0, j = 0, length = s.size();
    while (i < length) {
        size_t eol;
        while (i < length && s[i] != '\r' && s[i] != '\n') {
            ++i;
        }
        eol = i;
        if (i < length) {
            if (s[i] == '\r' && i + 1 < length && s[i + 1] == '\n') {
                i += 2;
            } else {
                ++i;
            }
            if (keepends) {
                eol = i;
            }
        }
        result.push_back(s.substr(j, eol - j));
        j = i;
    }
    return result;
}

std::string StrUtil::translate(const std::string &s, const std::array<char, 256> &table,
                               const std::vector<char> &deleteChars) {
    std::array<int, 256> transTable{};
    for (size_t i = 0; i < 256; ++i) {
        transTable[i] = static_cast<int>(table[i]);
    }
    for (auto c: deleteChars) {
        transTable[static_cast<size_t >(c)] = -1;
    }
    std::string result;
    for (auto c: s) {
        if (transTable[static_cast<size_t>(c)] != -1) {
            result.push_back(static_cast<char>(transTable[static_cast<size_t>(c)]));
        }
    }
    return result;
}

std::string StrUtil::filter(const std::string &s, std::function<bool(char)> pred) {
    std::string result;
    for (char c: s) {
        if (pred(c)) {
            result.push_back(c);
        }
    }
    return result;
}