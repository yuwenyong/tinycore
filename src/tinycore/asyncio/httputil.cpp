//
// Created by yuwenyong on 17-3-29.
//

#include "tinycore/asyncio/httputil.h"
#include <boost/algorithm/string.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/logging/log.h"
#include "tinycore/utilities/string.h"


const boost::regex HTTPHeaders::_normalizedHeader("^[A-Z0-9][a-z0-9]*(-[A-Z0-9][a-z0-9]*)*$");

void HTTPHeaders::add(const std::string &name, const std::string &value) {
    std::string normName = normalizeName(name);
    if (has(normName)) {
        _items[normName] += ',' + value;
        _asList[normName].emplace_back(value);
    } else {
        (*this)[normName] = value;
    }
}

StringVector HTTPHeaders::getList(const std::string &name) const {
    std::string normName = normalizeName(name);
    auto iter = _asList.find(normName);
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
    return add(name, value);
}

void HTTPHeaders::erase(const std::string &name) {
    std::string normName = HTTPHeaders::normalizeName(name);
    if (!has(normName)) {
        ThrowException(KeyError, normName);
    }
    _items.erase(normName);
    _asList.erase(normName);
}

std::string HTTPHeaders::get(const std::string &name, const std::string &defaultValue) const {
    std::string normName = HTTPHeaders::normalizeName(name);
    auto iter = _items.find(normName);
    if (iter != _items.end()) {
        return iter->second;
    } else {
        return defaultValue;
    }
}

void HTTPHeaders::parseLines(const std::string &headers) {
    StringVector lines = String::splitLines(headers);
    for (auto &line: lines) {
        if (!line.empty()) {
            parseLine(line);
        }
    }
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


void HTTPUtil::parseMultipartFormData(std::string boundary, const ByteArray &data, QueryArgumentsType &arguments,
                                      RequestFilesType &files) {
    if (boost::starts_with(boundary, "\"") && boost::ends_with(boundary, "\"")) {
        if (boundary.length() >= 2) {
            boundary = boundary.substr(1, boundary.length() - 2);
        }
    }
    size_t footerLength;
    if (data.size() >= 2 && (char)data[data.size() - 2] == '\r' && (char)data[data.size() - 1] == '\n') {
        footerLength = boundary.length() + 6;
    } else {
        footerLength = boundary.length() + 4;
    }
    if (data.size() <= footerLength) {
        return;
    }
    std::string sep("--" + boundary + "\r\n");
    const Byte *beg = data.data(), *end = data.data() + data.size() - footerLength, *cur, *val;
    size_t eoh, eon, valSize;
    std::string part, nameHeader, name, nameValue, ctype;
    HTTPHeaders headers;
    StringMap nameValues;
    StringVector nameParts;
    decltype(nameValues.begin()) nameIter, fileNameIter;
    for (; true; beg = cur + sep.size()) {
        cur = std::search(beg, end, (const Byte *)sep.data(), (const Byte *)sep.data() + sep.size());
        if (cur == end) {
            break;
        }
        if (cur == beg) {
            continue;
        }
        part.assign(beg, cur);
        eoh = part.find("\r\n\r\n");
        if (eoh == std::string::npos) {
            Log::warn("multipart/form-data missing headers");
            continue;
        }
        headers.clear();
        headers.parseLines(part.substr(0, eoh));
        nameHeader = headers.get("Content-Disposition");
        if (!boost::starts_with(nameHeader, "form-data;") || !boost::ends_with(part, "\r\n")) {
            Log::warn("Invalid multipart/form-data");
            continue;
        }
        if (part.length() <= eoh + 6) {
            val = (const Byte *)part.data();
            valSize = 0;
        } else {
            val = (const Byte *)part.data() + eoh + 4;
            valSize = part.size() - eoh - 6;
        }
        nameValues.clear();
        nameHeader = nameHeader.substr(10);
        nameParts = String::split(nameHeader, ';');
        for (auto &namePart: nameParts) {
            boost::trim(namePart);
            eon = namePart.find('=');
            if (eon == std::string::npos) {
                ThrowException(ValueError, "need more than 2 values to unpack");
            }
            name = namePart.substr(0, eon);
            nameValue = namePart.substr(eon + 1);
            boost::trim(nameValue);
            nameValues.emplace(std::move(name), std::move(nameValue));
        }
        nameIter = nameValues.find("name");
        if (nameIter == nameValues.end()) {
            Log::warn("multipart/form-data value missing name");
            continue;
        }
        name = nameIter->second;
        fileNameIter = nameValues.find("filename");
        if (fileNameIter != nameValues.end()) {
            ctype = headers.get("Content-Type", "application/unknown");
            files[name].emplace_back(HTTPFile(std::move(fileNameIter->second), std::move(ctype),
                                              ByteArray(val, val + valSize)));
        } else {
            arguments[name].emplace_back((const char *)val, valSize);
        }
    }
}