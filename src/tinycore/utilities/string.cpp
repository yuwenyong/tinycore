//
// Created by yuwenyong on 17-3-30.
//

#include "tinycore/utilities/string.h"


StringVector String::split(const std::string &s, const std::string &delim, bool keepEmpty) {
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

StringVector String::splitLines(const std::string &s, bool keepends) {
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

int String::count(const std::string &s, char c) {
    int r = 0;
    std::string::size_type i = 0;
    while ((i = s.find(c, i)) != std::string::npos) {
        ++i;
        ++r;
    }
    return r;
}

int String::count(const std::string &s, const char *sub) {
    int r = 0;
    std::string::size_type i = 0, m = strlen(sub);
    while ((i = s.find(sub, i)) != std::string::npos) {
        i += m;
        ++r;
    }
    return r;
}

void String::capitalize(std::string &s) {
    size_t i, len = s.length();
    if (len > 0) {
        s[0] = (char)std::toupper(s[0]);
    }
    for (i = 1; i < len; ++i) {
        s[i] = (char)std::tolower(s[i]);
    }
}

std::string String::capitalizeCopy(const std::string &s) {
    size_t i, len = s.length();
    std::string result;
    result.reserve(len);
    if (len > 0) {
        if (std::islower(s[0])) {
            result.push_back((char)std::toupper(s[0]));
        } else {
            result.push_back(s[0]);
        }
    }
    for (i = 1; i < len; ++i) {
        if (std::isupper(s[i])) {
            result.push_back((char)std::tolower(s[i]));
        } else {
            result.push_back(s[i]);
        }
    }
    return result;
}

std::string String::formatUTCDate(time_t timeval, bool usegmt) {
    struct tm now;
    std::string zone;
    now = *gmtime(&timeval);
    if (usegmt) {
        zone = "GMT";
    } else {
        zone = "-0000";
    }
    const char *weekDays[] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    const char *months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    return format("%s, %02d %s %04d %02d:%02d:%02d %s", weekDays[now.tm_wday], now.tm_mday, months[now.tm_mon],
                  now.tm_yday + 1900, now.tm_hour, now.tm_min, now.tm_sec, zone.c_str());
}