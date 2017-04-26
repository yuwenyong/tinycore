//
// Created by yuwenyong on 17-3-29.
//

#ifndef TINYCORE_HTTPUTIL_H
#define TINYCORE_HTTPUTIL_H

#include "tinycore/common/common.h"
#include "tinycore/debugging/trace.h"
#include <boost/regex.hpp>


class HTTPHeaders;
typedef std::unique_ptr<HTTPHeaders> HTTPHeadersPtr;


class HTTPHeaders {
public:
    typedef std::map<std::string, StringVector> HeadersContainerType;
    typedef std::function<void (const std::string&, const std::string &)> CallbackType;

    HTTPHeaders() {}

    explicit HTTPHeaders(const StringMap &nameValues) {
        update(nameValues);
    }

    void add(const std::string &name, const std::string &value);
    StringVector getList(const std::string &name) const;

    void getAll(CallbackType callback) const {
        for (auto &name: _asList) {
            for (auto &value: name.second) {
                callback(name.first, value);
            }
        }
    }

    void parseLine(const std::string &line);

    void setItem(const std::string &name, const std::string &value);

    bool contain(const std::string &name) const {
        return _items.find(name) != _items.end();
    }

    const std::string& getItem(const std::string &name) const {
        return _items.at(HTTPHeaders::normalizeName(name));
    }

    void delItem(const std::string &name);
    const std::string& get(const std::string &name, const std::string &defaultValue="") const;

    void update(const StringMap &nameValues) {
        for(auto &nameValue: nameValues) {
            setItem(nameValue.first, nameValue.second);
        }
    }

    static HTTPHeadersPtr parse(const std::string &headers);
protected:
    static std::string normalizeName(const std::string& name);

    StringMap _items;
    HeadersContainerType _asList;
    static const boost::regex _normalizedHeader;
};


class HTTPFile {
public:
    HTTPFile(std::string fileName,
             std::string contentType,
             std::string body)
            : _fileName(std::move(fileName))
            , _contentType(std::move(contentType))
            , _body(std::move(body)) {

    }

    const std::string& getFileName() const {
        return _fileName;
    }

    const std::string& getContentType() const {
        return _contentType;
    }

    const std::string& getBody() const {
        return _body;
    }
protected:
    std::string _fileName;
    std::string _contentType;
    std::string _body;
};

#endif //TINYCORE_HTTPUTIL_H
