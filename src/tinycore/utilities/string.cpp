//
// Created by yuwenyong on 17-3-30.
//

#include "tinycore/utilities/string.h"


StringVector String::split(const std::string &s, const std::string &delim, bool keepEmtpy) {
    StringVector result;
    if (delim.empty()) {
        result.push_back(s);
        return result;
    }
    std::string::const_iterator beg = s.begin(), end;
    while (true) {
        end = std::search(beg, s.end(), delim.begin(), delim.end());
        std::string temp(beg, end);
        if (keepEmtpy || !temp.empty()) {
            result.push_back(temp);
        }
        if (end == s.end()) {
            break;
        }
        beg = end + delim.size();
    }
    return result;
}