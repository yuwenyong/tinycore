//
// Created by yuwenyong on 17-3-29.
//

#ifndef TINYCORE_HTTPUTIL_H
#define TINYCORE_HTTPUTIL_H

#include "tinycore/common/common.h"
#include "tinycore/debugging/trace.h"
#include <boost/regex.hpp>
#include <boost/iterator/iterator_facade.hpp>


class HTTPHeaders;
typedef std::unique_ptr<HTTPHeaders> HTTPHeadersPtr;


class HTTPHeadersTraits {
public:
    typedef std::vector<std::string> ValuesContainerType;
    typedef ValuesContainerType::const_iterator ValuesContainerIteratorType;
    typedef std::map<std::string, std::pair<std::string, ValuesContainerType>> HeadersContainerType;
    typedef HeadersContainerType::const_iterator HeadersContainerIteratorType;
    typedef std::pair<const std::string*, const std::string*> HeaderValueType;
};


class HTTPHeadersIterator
        : public boost::iterator<
                HTTPHeadersIterator,
                HTTPHeadersTraits::HeaderValueType,
                std::forward_iterator_tag
        > {
public:
    typedef HTTPHeadersTraits::HeaderValueType reference;

    HTTPHeadersIterator()
            : _headers(nullptr)
            , _values(nullptr) {

    }

    HTTPHeadersIterator(const HTTPHeadersTraits::HeadersContainerType *headers)
            : HTTPHeadersIterator(headers, headers->begin()) {
    }

    HTTPHeadersIterator(const HTTPHeadersTraits::HeadersContainerType *headers,
                        HTTPHeadersTraits::HeadersContainerIteratorType headerIter)
            : _headers(headers)
            , _headerIter(headerIter) {
        if (_headerIter == _headers->end()) {
            _values = nullptr;
        } else {
            _values = &(_headerIter->second.second);
            _valueIter = _values->begin();
            ASSERT(_valueIter != _values->end());
        }
        setup();
    }

    HTTPHeadersIterator(const HTTPHeadersTraits::HeadersContainerType *headers,
                        HTTPHeadersTraits::HeadersContainerIteratorType headerIter,
                        const HTTPHeadersTraits::ValuesContainerType *values)
            : _headers(headers)
            , _headerIter(headerIter)
            , _values(values) {
        if (values) {
            _valueIter = values->begin();
            ASSERT(_valueIter != _values->end());
        }
        setup();
    }

    HTTPHeadersIterator(const HTTPHeadersTraits::HeadersContainerType *headers,
                        HTTPHeadersTraits::HeadersContainerIteratorType headerIter,
                        const HTTPHeadersTraits::ValuesContainerType *values,
                        HTTPHeadersTraits::ValuesContainerIteratorType valueIter)
            : _headers(headers)
            , _headerIter(headerIter)
            , _values(values)
            , _valueIter(valueIter) {
        setup();

    }

    HTTPHeadersIterator(const HTTPHeadersIterator &other)
            : _headers(other._headers)
            , _headerIter(other._headerIter)
            , _values(other._values)
            , _valueIter(other._valueIter)
            , _headerValue(other._headerValue) {

    }

    HTTPHeadersIterator& operator=(const HTTPHeadersIterator &other) {
        _headers = other._headers;
        _headerIter = other._headerIter;
        _values = other._values;
        _valueIter = other._valueIter;
        _headerValue = other._headerValue;
    }

private:
    friend class boost::iterator_core_access;

    reference dereference() const {
        return _headerValue;
    }

    void increment() {
        ASSERT(_values);
        if (++_valueIter == _values->end()) {
            if (++_headerIter != _headers->end()) {
                _values = &(_headerIter->second.second);
                _valueIter = _values->begin();
                ASSERT(_valueIter != _values->end());
            } else {
                _values = nullptr;
            }
        }
        setup();
    }

    bool equal(const HTTPHeadersIterator &other) const {
        if (_headers != other._headers || _values != other._values) {
            return false;
        }
        if (!_values) {
            return true;
        }
        return _headerIter == other._headerIter && _valueIter == other._valueIter;
    }

    void setup() {
        if (!_values) {
            _headerValue.first = nullptr;
            _headerValue.second = nullptr;
        } else {
            _headerValue.first = &_headerIter->first;
            _headerValue.second = &(*_valueIter);
        }
    }

    const HTTPHeadersTraits::HeadersContainerType *_headers;
    HTTPHeadersTraits::HeadersContainerIteratorType _headerIter;
    const HTTPHeadersTraits::ValuesContainerType *_values;
    HTTPHeadersTraits::ValuesContainerIteratorType _valueIter;
    HTTPHeadersTraits::HeaderValueType _headerValue;
};


class HTTPHeaders {
public:
    typedef HTTPHeadersIterator Iterator;
    typedef HTTPHeadersTraits::HeadersContainerType HeadersContainerType;

    HTTPHeaders() {}

    HTTPHeaders(const StringMap &nameValues) {
        update(nameValues);
    }

    void add(std::string name, std::string value);
    StringVector getList(std::string name) const;

    Iterator begin() const {
        return Iterator(&_headers);
    }

    Iterator end() const {
        return Iterator(&_headers, _headers.end());
    }

    void parseLine(const std::string &line);
    void set(std::string name, std::string value);

    bool contain(const std::string &name) const {
        return _headers.find(name) != _headers.end();
    }

    const std::string& get(std::string name) const;
    void remove(std::string name);
    std::string getDefault(const std::string &name, const std::string &default_value={}) const;

    void update(const StringMap &nameValues) {
        for(auto &nameValue: nameValues) {
            set(nameValue.first, nameValue.second);
        }
    }

    static HTTPHeadersPtr parse(std::string headers);
protected:
    static std::string& normalizeName(std::string& name);

    HeadersContainerType _headers;
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
