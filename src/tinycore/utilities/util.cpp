//
// Created by yuwenyong on 17-9-6.
//

#include "tinycore/utilities/util.h"
#include "tinycore/utilities/strutil.h"


std::string DateTimeUtil::formatDate(const DateTime &timeval, bool usegmt) {
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
    return StrUtil::format("%s, %02d %s %04d %02d:%02d:%02d %s", weekdayNames[date.day_of_week().as_number()],
                           date.day(), monthNames[date.month() - 1], date.year(), time.hours(), time.minutes(),
                           time.seconds(), zone.c_str());
}

std::string DateTimeUtil::formatUTCDate(const DateTime &ts) {
    static std::locale loc(std::cout.getloc(), new boost::posix_time::time_facet("%a, %d %b %Y %H:%M:%S GMT"));
    std::stringstream ss;
    ss.imbue(loc);
    ss << ts;
    return ss.str();
}

DateTime DateTimeUtil::parseUTCDate(const std::string &date) {
    static std::locale loc(std::cin.getloc(), new boost::posix_time::time_input_facet("%a, %d %b %Y %H:%M:%S GMT"));
    std::istringstream ss(date);
    ss.imbue(loc);
    DateTime dt;
    ss >> dt;
    return dt;
}


std::string BinAscii::b2aHex(const ByteArray &s, bool reverse) {
    int init = 0;
    int end = (int)s.size();
    int op = 1;
    if (reverse) {
        init = (int)s.size() - 1;
        end = -1;
        op = -1;
    }
    std::ostringstream ss;
    for (int i = init; i != end; i += op) {
        char buffer[4];
        sprintf(buffer, "%02X", s[i]);
        ss << buffer;
    }
    return ss.str();
}

ByteArray BinAscii::a2bHex(const std::string &s, bool reverse) {
    ByteArray out;
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
        out.push_back((Byte)strtoul(buffer, NULL, 16));
    }
    return out;
}