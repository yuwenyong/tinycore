//
// Created by yuwenyong on 17-3-29.
//

#include "tinycore/networking/httputil.h"
#include <boost/algorithm/string.hpp>


const std::regex HTTPHeader::lineSep("\r\n");
const std::regex HTTPHeader::normalizedHeader("^[A-Z0-9][a-z0-9]*(-[A-Z0-9][a-z0-9]*)*$");


int HTTPHeader::parseLine(const std::string &line) {
    size_t pos = line.find(":");
    if (pos == 0 || pos == std::string::npos) {
        return -1;
    }
    std::string name = line.substr(0, pos);
    std::string value = line.substr(pos + 1, std::string::npos);
    boost::trim(value);
    if (value.empty()) {
        return -1;
    }
    return add(std::move(name), std::move(value));
}

int HTTPHeader::add(std::string name, std::string value) {
    HTTPHeader::normalizeName(name);
    _headers.insert(std::make_pair(std::move(name), std::move(value)));
    return 0;
}

StringVector HTTPHeader::getList(const std::string &name) const {
    StringVector values;
    auto range = _headers.equal_range(name);
    for (auto iter = range.first; iter != range.second; ++iter) {
        values.push_back(iter->second);
    }
    return values;
}


bool HTTPHeader::get(const std::string &name, std::string &value) const {
    auto iter = _headers.find(name);
    if (iter != _headers.end()) {
        value = iter->second;
        return true;
    } else {
        return false;
    }
}

std::string HTTPHeader::getDefault(const std::string &name, const std::string &default_value) const {
    auto iter = _headers.find(name);
    if (iter != _headers.end()) {
        return iter->second;
    } else {
        return default_value;
    }
}

void HTTPHeader::update(const HeaderContainer &values) {
    std::string name;
    for (auto &kv: values) {
        name = kv.first;
        HTTPHeader::normalizeName(name);
        _headers.insert(std::make_pair(std::move(name), kv.second));
    }
}


HTTPHeaderUPtr HTTPHeader::parse(std::string headers) {
    HTTPHeaderUPtr h = make_unique<HTTPHeader>();
    std::regex_token_iterator<std::string::iterator> rend;
    std::regex_token_iterator<std::string::iterator> pos(headers.begin(), headers.end(), HTTPHeader::lineSep, -1);
    std::string line;
    while (pos != rend) {
        line = *pos;
        if (!line.empty()) {
            h->parseLine(line);
        }
    }
    return h;
}

std::string& HTTPHeader::normalizeName(std::string &name) {
    if (!std::regex_match(name, HTTPHeader::normalizedHeader)) {
        bool needUpper = true;
        for (auto iter = name.begin(); iter != name.end(); ++iter) {
            if (*iter == '-') {
                needUpper = true;
            } else if (isalpha(*iter)) {
                if (needUpper) {
                    *iter = (char)toupper(*iter);
                    needUpper = false;
                } else {
                    *iter = (char)tolower(*iter);
                }
            }
        }
    }
    return name;
}
