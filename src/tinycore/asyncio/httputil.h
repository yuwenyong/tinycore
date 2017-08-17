//
// Created by yuwenyong on 17-3-29.
//

#ifndef TINYCORE_HTTPUTIL_H
#define TINYCORE_HTTPUTIL_H

#include "tinycore/common/common.h"
#include <boost/regex.hpp>
#include "tinycore/compress/zlib.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/httputils/httplib.h"
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

    HTTPHeaders(std::initializer_list<NameValueType> nameValues) {
        update(nameValues);
    }

    explicit HTTPHeaders(const StringMap &nameValues) {
        update(nameValues);
    }

    void add(const std::string &name, const std::string &value);
    StringVector getList(const std::string &name) const;

    void getAll(const CallbackType &callback) const {
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

    static void parseBodyArguments(const std::string &contentType, const std::string &body, QueryArgListMap &arguments,
                                   HTTPFileListMap &files);

    static void parseMultipartFormData(std::string boundary, const std::string &data, QueryArgListMap &arguments,
                                       HTTPFileListMap &files);

    static std::string formatTimestamp(const DateTime &ts);

    static std::string formatTimestamp(time_t ts) {
        return formatTimestamp(boost::posix_time::from_time_t(ts));
    }

    static std::string getHTTPReason(int statusCode) {
        return HTTP_RESPONSES.find(statusCode) != HTTP_RESPONSES.end() ? HTTP_RESPONSES.at(statusCode) : "Unknown";
    }
protected:
    static StringVector parseParam(std::string s);
    static std::tuple<std::string, StringMap> parseHeader(const std::string &line);
};


class GzipDecompressor {
public:
    GzipDecompressor()
            : _decompressObj(16 + Zlib::maxWBits) {

    }

    ByteArray decompress(const Byte *data, size_t len) {
        return _decompressObj.decompress(data, len);
    }

    ByteArray decompress(const ByteArray &data) {
        return _decompressObj.decompress(data);
    }

    ByteArray decompress(const std::string &data) {
        return _decompressObj.decompress(data);
    }

    std::string decompressToString(const Byte *data, size_t len) {
        return _decompressObj.decompressToString(data, len);
    }

    std::string decompressToString(const ByteArray &data) {
        return _decompressObj.decompressToString(data);
    }

    std::string decompressToString(const std::string &data) {
        return _decompressObj.decompressToString(data);
    }

    ByteArray flush() {
        return _decompressObj.flush();
    }

    std::string flushToString() {
        return _decompressObj.flushToString();
    }
protected:
    DecompressObj _decompressObj;
};


#endif //TINYCORE_HTTPUTIL_H
