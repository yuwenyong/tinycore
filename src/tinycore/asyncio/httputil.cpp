//
// Created by yuwenyong on 17-3-29.
//

#include "tinycore/asyncio/httputil.h"
#include <boost/algorithm/string.hpp>
#include "tinycore/asyncio/logutil.h"
#include "tinycore/common/errors.h"
#include "tinycore/utilities/string.h"


const boost::regex HTTPHeaders::_normalizedHeader("^[A-Z0-9][a-z0-9]*(-[A-Z0-9][a-z0-9]*)*$");

void HTTPHeaders::add(const std::string &name, const std::string &value) {
    std::string normName = normalizeName(name);
    _lastKey = normName;
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
    ASSERT(!line.empty());
    if (std::isspace(line[0])) {
        std::string newPart = " " + boost::trim_left_copy(line);
        auto &asList = _asList.at(_lastKey);
        if (asList.empty()) {
            ThrowException(IndexError, "list index out of range");
        }
        asList.back() += newPart;
        _items.at(_lastKey) += newPart;
    } else {
        size_t pos = line.find(':');
        if (pos == 0 || pos == std::string::npos) {
            ThrowException(ValueError, "Need more than 1 value to unpack");
        }
        std::string name = line.substr(0, pos);
        std::string value = line.substr(pos + 1, std::string::npos);
        boost::trim(value);
        add(name, value);
    }
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

void HTTPUtil::parseBodyArguments(const std::string &contentType, const std::string &body, QueryArgListMap &arguments,
                                  HTTPFileListMap &files) {
    if (boost::starts_with(contentType, "application/x-www-form-urlencoded")) {
        auto uriArguments = URLParse::parseQS(body, true);
        for (auto &nv: uriArguments) {
            if (!nv.second.empty()) {
                for (auto &value: nv.second) {
                    arguments[nv.first].push_back(std::move(value));
                }
            }
        }
    } else if (boost::starts_with(contentType, "multipart/form-data")) {
        StringVector fields = String::split(contentType, ';');
        bool found = false;
        std::string k, sep, v;
        for (auto &field: fields) {
            boost::trim(field);
            std::tie(k, sep, v) = String::partition(field, "=");
            if (k == "boundary" && !v.empty()) {
                HTTPUtil::parseMultipartFormData(std::move(v), body, arguments, files);
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_WARNING(gGenLog, "Invalid multipart/form-data");
        }
    }
}

void HTTPUtil::parseMultipartFormData(std::string boundary, const std::string &data, QueryArgListMap &arguments,
                                      HTTPFileListMap &files) {
    if (boost::starts_with(boundary, "\"") && boost::ends_with(boundary, "\"")) {
        if (boundary.length() >= 2) {
            boundary = boundary.substr(1, boundary.length() - 2);
        } else {
            boundary.clear();
        }
    }
    size_t finalBoundaryIndex = data.rfind("--" + boundary + "--");
    if (finalBoundaryIndex == std::string::npos) {
        LOG_WARNING(gGenLog, "Invalid multipart/form-data: no final boundary");
        return;
    }
    StringVector parts = String::split(data.substr(0, finalBoundaryIndex), "--" + boundary + "\r\n");
    size_t eoh;
    HTTPHeaders headers;
    std::string dispHeader, disposition, name, value, ctype;
    StringMap dispParams;
    decltype(dispParams.begin()) nameIter, fileNameIter;
    for (auto &part: parts) {
        if (part.empty()) {
            continue;
        }
        eoh = part.find("\r\n\r\n");
        if (eoh == std::string::npos) {
            LOG_WARNING(gGenLog, "multipart/form-data missing headers");
            continue;
        }
        headers.clear();
        headers.parseLines(part.substr(0, eoh));
        dispHeader = headers.get("Content-Disposition");
        std::tie(disposition, dispParams) = parseHeader(dispHeader);
        if (disposition != "form-data" || !boost::ends_with(part, "\r\n")) {
            LOG_WARNING(gGenLog, "Invalid multipart/form-data");
            continue;
        }
        if (part.length() <= eoh + 6) {
            value.clear();
        } else {
            value = part.substr(eoh + 4, part.length() - eoh - 6);
        }
        nameIter = dispParams.find("name");
        if (nameIter == dispParams.end()) {
            LOG_WARNING(gGenLog, "multipart/form-data value missing name");
            continue;
        }
        name = nameIter->second;
        fileNameIter = dispParams.find("filename");
        if (fileNameIter != dispParams.end()) {
            ctype = headers.get("Content-Type", "application/unknown");
            files[name].emplace_back(HTTPFile(std::move(fileNameIter->second), std::move(ctype), std::move(value)));
        } else {
            arguments[name].emplace_back(std::move(value));
        }
    }
}

std::string HTTPUtil::formatTimestamp(const DateTime &ts) {
    static std::locale loc(std::cout.getloc(), new boost::posix_time::time_facet("%a, %d %b %Y %H:%M:%S GMT"));
    std::stringstream ss;
    ss.imbue(loc);
    ss << ts;
    return ss.str();
}

StringVector HTTPUtil::parseParam(std::string s) {
    StringVector parts;
    size_t end;
    while (!s.empty() && s[0] == ';') {
        s.erase(s.begin());
        end = s.find(';');
        while (end != std::string::npos
               && ((String::count(s, '"', 0, end) - String::count(s, "\\\"", 0, end)) % 2 != 0)) {
            end = s.find('"', end + 1);
        }
        if (end == std::string::npos) {
            end = s.size();
        }
        parts.emplace_back(boost::trim_copy(s.substr(0, end)));
        s = s.substr(end);
    }
    return parts;
}

std::tuple<std::string, StringMap> HTTPUtil::parseHeader(const std::string &line) {
    StringVector parts = parseParam(";" + line);
    std::string key = std::move(parts[0]);
    parts.erase(parts.begin());
    StringMap pdict;
    size_t i;
    std::string name, value;
    for (auto &p: parts) {
        i = p.find('=');
        if (i != std::string::npos) {
            name = boost::to_lower_copy(boost::trim_copy(p.substr(0, i)));
            value = boost::trim_copy(p.substr(i + 1));
            if (value.size() >= 2 && value.front() == '\"' && value.back() == '\"') {
                value = value.substr(1, value.size() - 2);
                boost::replace_all(value, "\\\\", "\\");
                boost::replace_all(value, "\\\"", "\"");
            }
            pdict[std::move(name)] = std::move(value);
        }
    }
    return std::make_tuple(std::move(key), std::move(pdict));
}