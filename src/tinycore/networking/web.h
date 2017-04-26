//
// Created by yuwenyong on 17-4-17.
//

#ifndef TINYCORE_WEB_H
#define TINYCORE_WEB_H

#include "tinycore/common/common.h"
#include <boost/functional/factory.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/any.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/optional.hpp>
#include "tinycore/common/errors.h"
#include "tinycore/compress/gzip.h"
#include "tinycore/networking/httpserver.h"
#include "tinycore/utilities/cookie.h"
#include "tinycore/utilities/httplib.h"


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

    friend class Application;

    RequestHandler(Application *application, HTTPRequestPtr request);
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

    template <typename T>
    void setHeader(const std::string &name, T &&value) {
        _headers[name] = std::to_string(std::forward<T>(value));
    }

    std::string getArgument(const std::string &name, bool strip= true) const;
    std::string getArgument(const std::string &name, const std::string &defaultValue, bool strip= true) const;
    StringVector getArguments(const std::string &name, bool strip= true) const;
    const BaseCookie& cookies();

    const std::string& getCookie(const std::string &name, const std::string &defaultValue= {}) {
        if (cookies().contain(name)) {
            return cookies().getItem(name).getValue();
        }
        return defaultValue;
    }

    void setCookie(const std::string &name, const std::string &value, const char *domain= nullptr,
                   const DateTime *expires= nullptr, const char *path= "/", int *expiresDays= nullptr,
                   const StringMap *args= nullptr);

    void clearCookie(const std::string &name, const char *path= "/", const char *domain= nullptr) {
        DateTime expires = boost::posix_time::second_clock::universal_time() + boost::gregorian::days(365);
        setCookie(name, "", domain, &expires, path);
    }

    void clearAllCookies() {
        cookies().getAll([this](const std::string &name, const Morsel &morsel) {
            clearCookie(name);
        });
    }

    void redirect(const std::string &url, bool permanent=false);
    void finish() {}
protected:
    void execute(TransformsType &transforms, StringVector args) {

    }

    std::string requestSummary() const {
        return "";
    }

    Application *_application;
    HTTPRequestPtr _request;
    bool _headersWritten{false};
    bool _finished{false};
    bool _autoFinish{true};
    TransformsType _transforms;
    StringMap _headers;
    MessageBuffer _writeBuffer;
    int _statusCode;
    CookiesType _cookies;
    NewCookiesType _newCookies;
};

typedef std::shared_ptr<RequestHandler> RequestHandlerPtr;
typedef std::weak_ptr<RequestHandler> RequestHandlerWPtr;


template <typename T>
class RequestHandlerFactory {
public:
    typedef RequestHandler::ArgsType ArgsType;

    RequestHandlerPtr operator()(Application *application, HTTPRequestPtr request, ArgsType &args) const {
        auto requestHandler = std::make_shared<T>(application, std::move(request));
        requestHandler->start(args);
        return requestHandler;
    }
};


class TC_COMMON_API Application {
public:
    typedef boost::ptr_vector<URLSpec> HandlersType;
    typedef boost::xpressive::sregex HostPatternType;
    typedef std::pair<HostPatternType, HandlersType> HostHandlerType;
    typedef boost::ptr_vector<HostHandlerType> HostHandlersType;
    typedef std::map<std::string, URLSpec *> NamedHandlersType;
    typedef std::map<std::string, boost::any> SettingsType;
    typedef std::function<OutputTransform* (HTTPRequestPtr)> TransformType;
    typedef std::vector<TransformType> TransformsType;
    typedef std::function<void (RequestHandler *)> LogFunctionType;

    Application(HandlersType &handlers=defaultHandlers, std::string defaultHost="", TransformsType transforms={},
                SettingsType settings={});
    virtual ~Application();

    void addHandlers(std::string hostPattern, HandlersType &hostHandlers);

    template <typename... Args>
    HTTPServerPtr listen(unsigned short port, std::string address, Args&&... args) {
        HTTPServerPtr server = make_unique([this](HTTPRequestPtr request) {
            (*this)(std::move(request));
        }, std::forward<Args>(args)...);
        server->listen(port, std::move(address));
        return server;
    }

    void addTransform(TransformType transformClass) {
        _transforms.push_back(std::move(transformClass));
    }

    template <typename T>
    void addTransform();

    void operator()(HTTPRequestPtr request);

    template <typename... Args>
    std::string reverseURL(const std::string &name, Args&&... args);

    void logRequest(RequestHandler *handler) const;

    const SettingsType& getSettings() const {
        return _settings;
    }

    static HandlersType defaultHandlers;
protected:
    HandlersType* getHostHandlers(HTTPRequestPtr request);

    TransformsType _transforms;
    HostHandlersType _handlers;
    NamedHandlersType _namedHandlers;
    std::string _defaultHost;
    SettingsType _settings;
};


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
    virtual void transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) =0;
    virtual void transformChunk(std::vector<char> &chunk, bool finishing) =0;
};


class TC_COMMON_API GZipContentEncoding: public OutputTransform {
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


class TC_COMMON_API ChunkedTransferEncoding: public OutputTransform {
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
        return _factory(request);
    }

protected:
    boost::factory<T*> _factory;
};


class TC_COMMON_API URLSpec {
public:
    typedef boost::xpressive::sregex RegexType;
    typedef RequestHandler::ArgsType ArgsType;
    typedef std::function<RequestHandlerPtr (Application*, HTTPRequestPtr, ArgsType&)> HandlerClassType;

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

using url = URLSpec;

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

#endif //TINYCORE_WEB_H
