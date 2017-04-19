//
// Created by yuwenyong on 17-4-17.
//

#ifndef TINYCORE_WEB_H
#define TINYCORE_WEB_H

#include <boost/functional/factory.hpp>
#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"
#include "tinycore/compress/gzip.h"
#include "tinycore/networking/httpserver.h"
#include "tinycore/utilities/httplib.h"



//class RequestHandler: public std::enable_shared_from_this<RequestHandler> {
//public:
//    virtual ~RequestHandler();
//};
//
//
//class Application {
//public:
//
//};

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

#endif //TINYCORE_WEB_H
