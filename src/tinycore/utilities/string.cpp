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

int String::count(const std::string &s, std::function<bool(char)> pred) {
    int r = 0;
    for (char c: s) {
        if (pred(c)) {
            ++r;
        }
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

std::string String::formatUTCDate(const DateTime &timeval, bool usegmt) {
    std::string zone;
    if (usegmt) {
        zone = "GMT";
    } else {
        zone = "-0000";
    }
    const char *weekdayNames[] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    const char *monthNames[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const Date date = timeval.date();
    const Time time = timeval.time_of_day();
    return format("%s, %02d %s %04d %02d:%02d:%02d %s", weekdayNames[date.day_of_week().as_number()],
                  date.day(), monthNames[date.month()], date.year(), time.hours(), time.minutes(), time.seconds(),
                  zone.c_str());
}

std::string String::translate(const std::string &s, const std::array<char, 256> &table,
                              const std::vector<char> &deleteChars) {
    std::array<int, 256> transTable;
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

std::string String::toHexStr(const std::string &s, bool reverse) {
    int init = 0;
    int end = (int)s.length();
    int op = 1;
    if (reverse) {
        init = (int)s.length() - 1;
        end = -1;
        op = -1;
    }
    std::ostringstream ss;
    for (int i = init; i != end; i += op) {
        char buffer[4];
        printf("---------\n");
        printf("%d",(int)(s[i]));
        sprintf(buffer, "%02X", (int)s[i]);
        ss << buffer;
    }
    return ss.str();
}

std::string String::fromHexStr(const std::string &s, bool reverse) {
    std::string out;
    if (s.length() & 1) {
        return out;
    }
    int init = 0;
    int end = (int)s.length();
    int op = 1;
    if (reverse) {
        init = (int)s.length() - 2;
        end = -2;
        op = -1;
    }
    for (int i = init; i != end; i += 2 * op) {
        char buffer[3] = { s[i], s[i + 1], '\0' };
        out.push_back((char)strtoul(buffer, NULL, 16));
    }
    return out;
}

std::string String::filter(const std::string &s, std::function<bool(char)> pred) {
    std::string result;
    for (char c: s) {
        if (pred(c)) {
            result.push_back(c);
        }
    }
    return result;
}
