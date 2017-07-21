//
// Created by yuwenyong on 17-3-29.
//

#ifndef TINYCORE_HTTPUTIL_H
#define TINYCORE_HTTPUTIL_H

#include "tinycore/common/common.h"
#include <boost/regex.hpp>
#include "tinycore/debugging/trace.h"
#include "tinycore/httputils/urlparse.h"


class HTTPHeaders {
public:
    typedef std::pair<std::string, std::string> NameValueType;
    typedef std::map<std::string, StringVector> HeadersContainerType;
    typedef std::function<void (const std::string&, const std::string&)> CallbackType;

    class HTTPHeadersSetter {
    public:
        HTTPHeadersSetter(std::string *value, StringVector *values)
                : _value(value)
                , _values(values) {
        }

        HTTPHeadersSetter& operator=(const std::string &value) {
            *_value = value;
            *_values = {value, };
            return *this;
        }

        operator std::string() const {
            return *_value;
        }
    protected:
        std::string *_value{nullptr};
        StringVector *_values{nullptr};
    };

    HTTPHeaders() {}

    explicit HTTPHeaders(std::initializer_list<NameValueType> nameValues) {
        update(nameValues);
    }

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

    HTTPHeadersSetter operator[](const std::string &name) {
        std::string normName = HTTPHeaders::normalizeName(name);
        return HTTPHeadersSetter(&_items[normName], &_asList[normName]);
    }

    bool has(const std::string &name) const {
        std::string normName = HTTPHeaders::normalizeName(name);
        return _items.find(name) != _items.end();
    }

    const std::string& at(const std::string &name) const {
        return _items.at(HTTPHeaders::normalizeName(name));
    }

    void erase(const std::string &name);
    std::string get(const std::string &name, const std::string &defaultValue="") const;

    void update(std::initializer_list<NameValueType> nameValues) {
        for(auto &nameValue: nameValues) {
            (*this)[nameValue.first] = nameValue.second;
        }
    }

    void update(const StringMap &nameValues) {
        for(auto &nameValue: nameValues) {
            (*this)[nameValue.first] = nameValue.second;
        }
    }

    void clear() {
        _items.clear();
        _asList.clear();
    }

    void parseLines(const std::string &headers);

    static std::unique_ptr<HTTPHeaders> parse(const std::string &headers) {
        auto h = HTTPHeaders::create();
        h->parseLines(headers);
        return h;
    }

    template <typename ...Args>
    static std::unique_ptr<HTTPHeaders> create(Args&& ...args) {
        return make_unique<HTTPHeaders>(std::forward<Args>(args)...);
    }
protected:
    static std::string normalizeName(const std::string &name);

    StringMap _items;
    HeadersContainerType _asList;
    std::string _lastKey;
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


using HTTPFileListMap = std::map<std::string, std::vector<HTTPFile>>;


class HTTPUtil {
public:
    template <typename ArgsType>
    static std::string urlConcat(std::string url, const ArgsType &args) {
        if (args.empty()) {
            return url;
        }
        if (url.empty() || (url.back() != '?' && url.back() != '&')) {
            if (url.find('?') != std::string::npos) {
                url.push_back('&');
            } else {
                url.push_back('?');
            }
        }
        url += URLParse::urlEncode(args);
        return url;
    }

    static void parseMultipartFormData(std::string boundary, const std::string &data, QueryArgListMap &arguments,
                                       HTTPFileListMap &files);

protected:
    static StringVector parseParam(std::string s);
    static std::tuple<std::string, StringMap> parseHeader(const std::string &line);
};


#endif //TINYCORE_HTTPUTIL_H
