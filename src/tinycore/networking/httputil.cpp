//
// Created by yuwenyong on 17-3-29.
//

#include "tinycore/networking/httputil.h"
#include <boost/algorithm/string.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/utilities/string.h"


const std::regex HTTPHeaders::_normalizedHeader("^[A-Z0-9][a-z0-9]*(-[A-Z0-9][a-z0-9]*)*$");

void HTTPHeaders::add(std::string name, std::string value) {
    normalizeName(name);
    auto iter = _headers.find(name);
    if (iter != _headers.end()) {
        iter->second.first += ',' + value;
        iter->second.second.push_back(std::move(value));
    } else {
        _headers[name] = std::make_pair(std::move(name), {std::move(value), });
    }
}

StringVector HTTPHeaders::getList(std::string name) const {
    normalizeName(name);
    auto iter = _headers.find(name);
    if (iter != _headers.end()) {
        return iter->second.second;
    } else {
        return {};
    }
}

void HTTPHeaders::parseLine(const std::string &line) {
    size_t pos = line.find(":");
    if (pos == 0 || pos == std::string::npos) {
        ThrowException(ValueError, "Need more than 1 value to unpack");
    }
    std::string name = line.substr(0, pos);
    std::string value = line.substr(pos + 1, std::string::npos);
    boost::trim(value);
    return add(std::move(name), std::move(value));
}

void HTTPHeaders::set(std::string name, std::string value) {
    HTTPHeaders::normalizeName(name);
    _headers[name] = std::make_pair(std::move(name), {std::move(value), });
}

const std::string& HTTPHeaders::get(std::string name) const {
    HTTPHeaders::normalizeName(name);
    auto iter = _headers.find(name);
    if (iter == _headers.end()) {
        ThrowException(KeyError, "Key Error:" + name);
    }
    return iter->second.first;
}

void HTTPHeaders::remove(std::string name) {
    HTTPHeaders::normalizeName(name);
    auto iter = _headers.find(name);
    if (iter == _headers.end()) {
        ThrowException(KeyError, "Key Error:" + name);
    }
    _headers.erase(iter);
}

std::string HTTPHeaders::getDefault(const std::string &name, const std::string &default_value) const {
    auto iter = _headers.find(name);
    if (iter != _headers.end()) {
        return iter->second.first;
    } else {
        return default_value;
    }
}


HTTPHeadersPtr HTTPHeaders::parse(std::string headers) {
    HTTPHeadersPtr h = make_unique<HTTPHeaders>();
    StringVector lines = String::splitLines(headers);
    for (auto &line: lines) {
        if (!line.empty()) {
            h->parseLine(line);
        }
    }
    return h;
}

std::string& HTTPHeaders::normalizeName(std::string &name) {
    if (!std::regex_match(name, HTTPHeaders::_normalizedHeader)) {
        StringVector nameParts = String::split(name, '-');
        for (auto &namePart: nameParts) {
            String::capitalize(namePart);
        }
        name = boost::join(nameParts, "-");
    }
    return name;
}
