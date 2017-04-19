//
// Created by yuwenyong on 17-4-17.
//

#ifndef TINYCORE_WEB_H
#define TINYCORE_WEB_H

#include <boost/functional/factory.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/any.hpp>
#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"
#include "tinycore/compress/gzip.h"
#include "tinycore/networking/httpserver.h"
#include "tinycore/utilities/httplib.h"


class Application;


class RequestHandler: public std::enable_shared_from_this<RequestHandler> {
public:
    typedef std::map<std::string, boost::any> ArgsType;

    RequestHandler(Application *application, HTTPRequestPtr request, ArgsType &args);
    virtual ~RequestHandler();
};

typedef std::shared_ptr<RequestHandler> RequestHandlerPtr;

template <typename T>
class RequestHandlerFactory {
public:
    typedef RequestHandler::ArgsType ArgsType;

    RequestHandlerPtr operator()(Application *application, HTTPRequestPtr request, ArgsType &args) {
        return std::make_shared<T>(application, std::move(request), args);
    }
};


class Application {
public:

};

class HTTPError: public Exception {
public:
    HTTPError(const char *file, int line, const char *func, HTTPStatusCode statusCode)
            : HTTPError(file, line, func, statusCode, "") {

    }

    HTTPError(const char *file, int line, const char *func, HTTPStatusCode statusCode,const std::string &message)
            : Exception(file, line, func, message)
            , _statusCode(statusCode) {

    }

    const char *what() const noexcept override;

    virtual const char *getTypeName() const override {
        return "HTTPError";
    }
protected:
    HTTPStatusCode _statusCode;
};


class OutputTransform {
public:
    virtual ~OutputTransform() {}
    virtual void transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) =0;
    virtual void transformChunk(std::vector<char> &chunk, bool finishing) =0;
};


class GZipContentEncoding: public OutputTransform {
public:
    GZipContentEncoding(HTTPRequestPtr request);
    void transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) override;
    void transformChunk(std::vector<char> &chunk, bool finishing) override;

    static const StringSet CONTENT_TYPES;
    static const int MIN_LENGTH = 5;
protected:
    bool _gzipping;
    std::vector<char> _gzipValue;
    GZipCompressor _gzipFile;
    size_t _gzipPos{0};
};


class ChunkedTransferEncoding: public OutputTransform {
public:
    ChunkedTransferEncoding(HTTPRequestPtr request) {
        _chunking = request->supportsHTTP1_1();
    }

    void transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) override;
    void transformChunk(std::vector<char> &chunk, bool finishing) override;

protected:
    bool _chunking;
};


template <typename T>
class OutputTransformFactory {
public:
    OutputTransform* operator()(HTTPRequestPtr request) {
        return _factory(std::move(request));
    }

protected:
    boost::factory<T*> _factory;
};


class URLSpec {
public:
    typedef boost::xpressive::sregex RegexType;
    typedef RequestHandler::ArgsType ArgsType;
    typedef std::function<RequestHandlerPtr (Application*, HTTPRequestPtr, ArgsType&)> HandlerClassType;

    URLSpec(std::string pattern, HandlerClassType handlerClass, ArgsType args={}, std::string name={});

    template <typename... Args>
    std::string reverse(Args&&... args) {
        ASSERT(!_path.empty(), "Cannot reverse url regex");
        ASSERT(sizeof...(Args) == _groupCount, "required number of arguments not found");
        return String::format(_path.c_str(), std::forward<Args>(args)...);
    }

    std::string reverse() {
        ASSERT(!_path.empty(), "Cannot reverse url regex");
        ASSERT(_groupCount == 0, "required number of arguments not found");
        return _path;
    }
protected:
    std::tuple<std::string, int> findGroups();

    std::string _pattern;
    RegexType _regex;
    HandlerClassType _handlerClass;
    ArgsType _args;
    std::string _name;
    std::string _path;
    int _groupCount;
};

using url = URLSpec;

#endif //TINYCORE_WEB_H
