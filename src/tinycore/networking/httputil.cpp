//
// Created by yuwenyong on 17-3-29.
//

#include "tinycore/networking/httputil.h"
#include <boost/algorithm/string.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/utilities/string.h"


const boost::regex HTTPHeaders::_normalizedHeader("^[A-Z0-9][a-z0-9]*(-[A-Z0-9][a-z0-9]*)*$");

void HTTPHeaders::add(const std::string &name, const std::string &value) {
    std::string normName = normalizeName(name);
    if (contain(normName)) {
        _items[normName] += ',' + value;
        _asList[normName].emplace_back(value);
    } else {
        setItem(std::move(normName), value);
    }
}

StringVector HTTPHeaders::getList(const std::string &name) const {
    std::string normName = normalizeName(name);
    auto iter = _asList.find(name);
    if (iter != _asList.end()) {
        return iter->second;
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

void HTTPHeaders::setItem(const std::string &name, const std::string &value) {
    std::string normName = HTTPHeaders::normalizeName(name);
    _items[normName] = value;
    _asList[normName] = {value, };
}

void HTTPHeaders::delItem(const std::string &name) {
    std::string normName = HTTPHeaders::normalizeName(name);
    if (!contain(normName)) {
        ThrowException(KeyError, normName);
    }
    _items.erase(name);
    _asList.erase(name);
}

const std::string& HTTPHeaders::get(const std::string &name, const std::string &defaultValue) const {
    std::string normName = HTTPHeaders::normalizeName(name);
    auto iter = _items.find(name);
    if (iter != _items.end()) {
        return iter->second;
    } else {
        return defaultValue;
    }
}

HTTPHeadersPtr HTTPHeaders::parse(const std::string &headers) {
    HTTPHeadersPtr h = make_unique<HTTPHeaders>();
    StringVector lines = String::splitLines(headers);
    for (auto &line: lines) {
        if (!line.empty()) {
            h->parseLine(line);
        }
    }
    return h;
}

std::string HTTPHeaders::normalizeName(const std::string &name) {

    if (boost::regex_match(name, HTTPHeaders::_normalizedHeader)) {
        return name;
    }
    StringVector nameParts = String::split(name, '-');
    for (auto &namePart: nameParts) {
        String::capitalize(namePart);
    }
    return boost::join(nameParts, "-");
}
