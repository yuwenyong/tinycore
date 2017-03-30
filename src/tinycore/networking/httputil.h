//
// Created by yuwenyong on 17-3-29.
//

#ifndef TINYCORE_HTTPUTIL_H
#define TINYCORE_HTTPUTIL_H

#include "tinycore/common/common.h"
#include <regex>

class HTTPHeader;
typedef std::unique_ptr<HTTPHeader> HTTPHeaderUPtr;

class HTTPHeader {
public:
    typedef std::multimap<std::string, std::string> HeaderContainer;

    HTTPHeader() {}
    ~HTTPHeader() {}

    int parseLine(const std::string &line);
    int add(std::string name, std::string value);
    StringVector getList(const std::string &name) const;

    const HeaderContainer& getAll() const {
        return _headers;
    }

    bool contain(const std::string &name) const {
        return _headers.find(name) != _headers.end();
    }

    bool get(const std::string &name, std::string &value) const;
    std::string getDefault(const std::string &name, const std::string &default_value={}) const;
    void update(const HeaderContainer &values);

    static HTTPHeaderUPtr parse(std::string headers);
    static std::string& normalizeName(std::string& name);

    static const std::regex lineSep;
    static const std::regex normalizedHeader;
protected:
    HeaderContainer _headers;
};


#endif //TINYCORE_HTTPUTIL_H
