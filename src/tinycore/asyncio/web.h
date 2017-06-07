//
// Created by yuwenyong on 17-4-17.
//

#ifndef TINYCORE_WEB_H
#define TINYCORE_WEB_H

#include "tinycore/common/common.h"
#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>
#include <boost/functional/factory.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/xpressive/xpressive.hpp>
#include "tinycore/asyncio/httpserver.h"
#include "tinycore/common/errors.h"
#include "tinycore/compress/gzip.h"
#include "tinycore/httputils/httplib.h"
#include "tinycore/utilities/container.h"
#include "tinycore/utilities/string.h"


class Application;
class URLSpec;
class OutputTransform;
class URLSpec;


class TC_COMMON_API RequestHandler: public std::enable_shared_from_this<RequestHandler> {
public:
    typedef std::map<std::string, boost::any> ArgsType;
    typedef boost::ptr_vector<OutputTransform> TransformsType;
    typedef std::map<std::string, boost::any> SettingsType;
    typedef boost::optional<BaseCookie> CookiesType;
    typedef boost::optional<std::vector<BaseCookie>> NewCookiesType;
    typedef boost::property_tree::ptree SimpleJSONType;

    friend class Application;

    RequestHandler(Application *application, std::shared_ptr<HTTPServerRequest> request);
    virtual ~RequestHandler();

    void start(ArgsType &args);
    virtual void initialize(ArgsType &args);
    const SettingsType& getSettings() const;
    virtual void onHead(StringVector args);
    virtual void onGet(StringVector args);
    virtual void onPost(StringVector args);
    virtual void onDelete(StringVector args);
    virtual void onPut(StringVector args);
    virtual void onOptions(StringVector args);

    virtual void prepare();
    virtual void onConnectionClose();
    void clear();

    void setStatus(int statusCode) {
        ASSERT(HTTPResponses.find(statusCode) != HTTPResponses.end());
        _statusCode = statusCode;
    }

    int getStatus() const {
        return _statusCode;
    }

    void setHeader(const std::string &name, const DateTime &value) {
        _headers[name] = String::formatUTCDate(value, true);
    }

    void setHeader(const std::string &name, const std::string &value);
    void setHeader(const std::string &name, const char *value);

//    template <typename T>
//    void setHeader(const std::string &name, T &&value) {
//        _headers[name] = std::to_string(std::forward<T>(value));
//    }

    template <typename T>
    void setHeader(const std::string &name, T value) {
        _headers[name] = std::to_string(value);
    }

    std::string getArgument(const std::string &name, const char *defaultValue= nullptr, bool strip= true) const;
    std::string getArgument(const std::string &name, const std::string &defaultValue, bool strip= true) const;
    StringVector getArguments(const std::string &name, bool strip= true) const;
    const BaseCookie& cookies();

    std::string getCookie(const std::string &name, const std::string &defaultValue= {}) {
        if (cookies().has(name)) {
            return cookies().at(name).getValue();
        }
        return defaultValue;
    }

    void setCookie(const std::string &name, const std::string &value, const char *domain= nullptr,
                   const DateTime *expires= nullptr, const char *path= "/", int *expiresDays= nullptr,
                   const StringMap *args= nullptr);

    void clearCookie(const std::string &name, const char *path= "/", const char *domain= nullptr) {
        DateTime expires = boost::posix_time::second_clock::universal_time() - boost::gregorian::days(365);
        setCookie(name, "", domain, &expires, path);
    }

    void clearAllCookies() {
        cookies().getAll([this](const std::string &name, const Morsel &morsel) {
            clearCookie(name);
        });
    }

    void redirect(const std::string &url, bool permanent=false);

    void write(const Byte *chunk, size_t length) {
        ASSERT(!_finished);
        _writeBuffer.insert(_writeBuffer.end(), chunk, chunk + length);
    }

    void write(const char *chunk) {
        write((const Byte *)chunk, strlen(chunk));
    }

    void write(const std::string &chunk) {
        write((const Byte *)chunk.data(), chunk.size());
    }

    void write(const ByteArray &chunk) {
        write(chunk.data(), chunk.size());
    }

    void write(const SimpleJSONType &chunk) {
        ASSERT(!_finished);
        setHeader("Content-Type", "application/json; charset=UTF-8");
        write(String::fromJSON(chunk));
    }

#ifdef HAS_RAPID_JSON
    void write(const rapidjson::Document &chunk) {
        ASSERT(!_finished);
        setHeader("Content-Type", "application/json; charset=UTF-8");
        write(String::fromJSON(chunk));
    }
#endif

    void flush(bool includeFooters= false);

    template <typename... Args>
    void finish(Args&&... args) {
        write(std::forward<Args>(args)...);
        finish();
    }

    void finish();
    void sendError(int statusCode = 500, std::exception *e= nullptr);
    virtual std::string getErrorHTML(int statusCode, std::exception *e= nullptr);
    void requireSetting(const std::string &name, const std::string &feature="this feature");

    template <typename... Args>
    std::string reverseURL(const std::string &name, Args&&... args);

    virtual std::string computeEtag() const;

    template <typename T>
    std::shared_ptr<T> getSelf() {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    static const StringSet supportedMethods;
protected:
    virtual void execute(TransformsType &transforms, StringVector args);
    std::string generateHeaders() const;
    void log();

    std::string requestSummary() const {
        return _request->getMethod() + " " + _request->getURI() + " (" + _request->getRemoteIp() + ")";
    }

    void handleRequestException(std::exception *e);

    Application *_application;
    std::shared_ptr<HTTPServerRequest> _request;
    bool _headersWritten{false};
    bool _finished{false};
    bool _autoFinish{true};
    TransformsType _transforms;
    StringMap _headers;
    ByteArray _writeBuffer;
    int _statusCode;
    CookiesType _cookies;
    NewCookiesType _newCookies;
};


template <typename T>
class RequestHandlerFactory {
public:
    typedef RequestHandler::ArgsType ArgsType;

    std::shared_ptr<RequestHandler> operator()(Application *application, std::shared_ptr<HTTPServerRequest> request,
                                               ArgsType &args) const {
        auto requestHandler = std::make_shared<T>(application, std::move(request));
        requestHandler->start(args);
        return requestHandler;
    }
};

#define Asynchronous() { \
    this->_autoFinish = false; \
}

#define RemoveSlash() { \
    const std::string &path = this->_request->getPath(); \
    if (boost::ends_with(path, "/")) { \
        const std::string &method = this->_request->getMethod(); \
        if (method == "GET" || method == "HEAD") { \
            std::string uri = boost::trim_right_copy_if(path, boost::is_any_of("/")); \
            if (!uri.empty()) { \
                const std::string &query = this->_request->getQuery(); \
                if (!query.empty()) { \
                    uri += "?" + query; \
                } \
                this->redirect(uri); \
                return; \
            } \
        } else { \
            ThrowException(HTTPError, 404); \
        } \
    } \
}

#define AddSlash() { \
    const std::string &path = this->_request->getPath(); \
    if (!boost::ends_with(path, "/")) { \
        const std::string &method = this->_request->getMethod(); \
        if (method == "GET" || method == "HEAD") { \
            std::string uri = path + "/"; \
            const std::string &query = this->_request->getQuery(); \
            if (!query.empty()) { \
                uri += "?" + query; \
            } \
            this->redirect(uri); \
            return; \
        } \
        ThrowException(HTTPError, 404); \
    } \
}


class TC_COMMON_API Application {
public:
    typedef PtrVector<URLSpec> HandlersType;
    typedef boost::xpressive::sregex HostPatternType;
    typedef std::pair<HostPatternType, HandlersType> HostHandlerType;
    typedef boost::ptr_vector<HostHandlerType> HostHandlersType;
    typedef std::map<std::string, URLSpec *> NamedHandlersType;
    typedef std::map<std::string, boost::any> SettingsType;
    typedef std::function<OutputTransform* (std::shared_ptr<HTTPServerRequest>)> TransformType;
    typedef std::vector<TransformType> TransformsType;
    typedef std::function<void (RequestHandler *)> LogFunctionType;

    Application(HandlersType &&handlers={}, std::string defaultHost="", TransformsType transforms={},
                SettingsType settings={});
    virtual ~Application();

    template <typename... Args>
    std::unique_ptr<HTTPServer> listen(unsigned short port, std::string address, Args&&... args) {
        std::unique_ptr<HTTPServer> server = make_unique<HTTPServer>(
                [this](std::shared_ptr<HTTPServerRequest> request) {
                    (*this)(std::move(request));
                }, std::forward<Args>(args)...);
        server->listen(port, std::move(address));
        return server;
    }

    void addHandlers(std::string hostPattern, HandlersType &&hostHandlers);

    void addTransform(TransformType transformClass) {
        _transforms.push_back(std::move(transformClass));
    }

    template <typename T>
    void addTransform();

    void operator()(std::shared_ptr<HTTPServerRequest> request);

    template <typename... Args>
    std::string reverseURL(const std::string &name, Args&&... args);

    void logRequest(RequestHandler *handler) const;

    const SettingsType& getSettings() const {
        return _settings;
    }

//    static HandlersType defaultHandlers;
protected:
    HandlersType* getHostHandlers(std::shared_ptr<HTTPServerRequest> request);

    TransformsType _transforms;
    HostHandlersType _handlers;
    NamedHandlersType _namedHandlers;
    std::string _defaultHost;
    SettingsType _settings;
};


#define HTTPServerCB(app) std::bind(&Application::operator(), pointer(app), std::placeholders::_1)


class TC_COMMON_API HTTPError: public Exception {
public:
    HTTPError(const char *file, int line, const char *func, int statusCode)
            : HTTPError(file, line, func, statusCode, "") {

    }

    HTTPError(const char *file, int line, const char *func, int statusCode,const std::string &message)
            : Exception(file, line, func, message)
            , _statusCode(statusCode) {

    }

    const char *what() const noexcept override;

    const char *getTypeName() const override {
        return "HTTPError";
    }

    int getStatusCode() const {
        return _statusCode;
    }
protected:
    int _statusCode;
};


class TC_COMMON_API ErrorHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override;
    void prepare() override;
};


class TC_COMMON_API RedirectHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override;
    void onGet(StringVector args) override;
protected:
    std::string _url;
    bool _permanent{true};
};


class TC_COMMON_API FallbackHandler: public RequestHandler {
public:
    typedef HTTPServer::RequestCallbackType FallbackType;

    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override;
    void prepare() override;
protected:
    FallbackType _fallback;
};


class TC_COMMON_API OutputTransform {
public:
    virtual ~OutputTransform() {}
    virtual void transformFirstChunk(StringMap &headers, ByteArray &chunk, bool finishing) =0;
    virtual void transformChunk(ByteArray &chunk, bool finishing) =0;
};


class TC_COMMON_API GZipContentEncoding: public OutputTransform {
public:
    GZipContentEncoding(std::shared_ptr<HTTPServerRequest> request);
    void transformFirstChunk(StringMap &headers, ByteArray &chunk, bool finishing) override;
    void transformChunk(ByteArray &chunk, bool finishing) override;

    static const StringSet contentTypes;
    static const int minLength = 5;
protected:
    bool _gzipping;
    std::shared_ptr<std::stringstream> _gzipValue;
    GzipFile _gzipFile;
    size_t _gzipPos{0};
};


class TC_COMMON_API ChunkedTransferEncoding: public OutputTransform {
public:
    ChunkedTransferEncoding(std::shared_ptr<HTTPServerRequest> request) {
        _chunking = request->supportsHTTP1_1();
    }

    void transformFirstChunk(StringMap &headers, ByteArray &chunk, bool finishing) override;
    void transformChunk(ByteArray &chunk, bool finishing) override;

protected:
    bool _chunking;
};


template <typename T>
class OutputTransformFactory {
public:
    OutputTransform* operator()(std::shared_ptr<HTTPServerRequest> request) {
        return _factory(request);
    }

protected:
    boost::factory<T*> _factory;
};


class TC_COMMON_API URLSpec: public boost::noncopyable {
public:
    typedef boost::xpressive::sregex RegexType;
    typedef RequestHandler::ArgsType ArgsType;
    typedef std::function<std::shared_ptr<RequestHandler> (Application*, std::shared_ptr<HTTPServerRequest>,
                                                           ArgsType&)> HandlerClassType;

    URLSpec(std::string pattern, HandlerClassType handlerClass, std::string name);
    URLSpec(std::string pattern, HandlerClassType handlerClass, ArgsType args={}, std::string name={});

    template <typename... Args>
    std::string reverse(Args&&... args) {
        ASSERT(!_path.empty(), "Cannot reverse url regex %s", _pattern.c_str());
        ASSERT(sizeof...(Args) == _groupCount, "required number of arguments not found");
        return String::format(_path.c_str(), std::forward<Args>(args)...);
    }

    std::string reverse() {
        ASSERT(!_path.empty(), "Cannot reverse url regex %s", _pattern.c_str());
        ASSERT(_groupCount == 0, "required number of arguments not found");
        return _path;
    }

    const RegexType& getRegex() const {
        return _regex;
    }

    const std::string& getPattern() const {
        return _pattern;
    }

    const std::string& getName() const {
        return _name;
    }

    HandlerClassType& getHandlerClass() {
        return _handlerClass;
    }

    ArgsType& getArgs() {
        return _args;
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


//#define url(pattern, HandlerClass, ...) boost::factory<URLSpec*>()(pattern, RequestHandlerFactory<HandlerClass>(), ##__VA_ARGS__)

template <typename HandlerClass, typename... Args>
URLSpec* url(const std::string &pattern, Args&&... args) {
    RequestHandlerFactory<HandlerClass> handlerFactory;
    return boost::factory<URLSpec*>()(pattern, handlerFactory, std::forward<Args>(args)...);
}


template <typename T>
void Application::addTransform() {
    _transforms.push_back(OutputTransformFactory<T>());
}

template <typename... Args>
std::string Application::reverseURL(const std::string &name, Args&&... args) {
    auto iter = _namedHandlers.find(name);
    if (iter == _namedHandlers.end()) {
        std::string error = name + " not found in named urls";
        ThrowException(KeyError, std::move(error));
    }
    return iter->second->reverse(std::forward<Args>(args)...);
}

template <typename... Args>
std::string RequestHandler::reverseURL(const std::string &name, Args&&... args) {
    return _application->reverseURL(name, std::forward<Args>(args)...);
}

#endif //TINYCORE_WEB_H
