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
//    typedef std::vector<std::pair<std::string, std::string>> ListHeadersType;
    typedef std::map<std::string, boost::any> SettingsType;
    typedef boost::optional<SimpleCookie> CookiesType;
//    typedef boost::optional<std::vector<SimpleCookie>> NewCookiesType;
    typedef boost::property_tree::ptree SimpleJSONType;
    typedef HTTPServerRequest::WriteCallbackType FlushCallbackType;

    friend class Application;

    RequestHandler(Application *application, std::shared_ptr<HTTPServerRequest> request);
    virtual ~RequestHandler();

    void start(ArgsType &args);
    virtual void initialize(ArgsType &args);
    const SettingsType& getSettings() const;
    virtual void onHead(const StringVector &args);
    virtual void onGet(const StringVector &args);
    virtual void onPost(const StringVector &args);
    virtual void onDelete(const StringVector &args);
    virtual void onPatch(const StringVector &args);
    virtual void onPut(const StringVector &args);
    virtual void onOptions(const StringVector &args);

    virtual void prepare();
    virtual void onFinish();
    virtual void onConnectionClose();
    void clear();
    virtual void setDefaultHeaders();

    void setStatus(int statusCode);

    void setStatus(int statusCode, const std::string &reason) {
        if (reason.empty()) {
            setStatus(statusCode);
        } else {
            _statusCode = statusCode;
            _reason = reason;
        }
    }

    int getStatus() const {
        return _statusCode;
    }

    void setHeader(const std::string &name, const std::string &value) {
        _headers[name] = convertHeaderValue(value);
    }

    void setHeader(const std::string &name, const char *value) {
        _headers[name] = convertHeaderValue(value);
    }

    void setHeader(const std::string &name, const DateTime &value) {
        _headers[name] = convertHeaderValue(value);
    }

    template <typename T>
    void setHeader(const std::string &name, T value) {
        _headers[name] = convertHeaderValue(value);
    }

    void addHeader(const std::string &name, const std::string &value) {
        _headers.add(name, convertHeaderValue(value));
    }

    void addHeader(const std::string &name, const char *value) {
        _headers.add(name, convertHeaderValue(value));
    }

    void addHeader(const std::string &name, const DateTime &value) {
        _headers.add(name, convertHeaderValue(value));
    }

    template <typename T>
    void addHeader(const std::string &name, T value) {
        _headers.add(name, convertHeaderValue(value));
    }

    void clearHeader(const std::string &name) {
        if (_headers.has(name)) {
            _headers.erase(name);
        }
    }

    bool hasArgument(const std::string &name) const {
        auto &arguments = _request->getArguments();
        return arguments.find(name) != arguments.end();
    }

    std::string getArgument(const std::string &name, const char *defaultValue= nullptr, bool strip= true) const {
        return getArgument(name, _request->getArguments(), defaultValue, strip);
    }

    StringVector getArguments(const std::string &name, bool strip= true) const {
        return getArguments(name, _request->getArguments(), strip);
    }

    bool hasBodyArgument(const std::string &name) const {
        auto &arguments = _request->getBodyArguments();
        return arguments.find(name) != arguments.end();
    }

    std::string getBodyArgument(const std::string &name, const char *defaultValue= nullptr, bool strip= true) const {
        return getArgument(name, _request->getBodyArguments(), defaultValue, strip);
    }

    StringVector getBodyArguments(const std::string &name, bool strip= true) const {
        return getArguments(name, _request->getBodyArguments(), strip);
    }

    bool hasQueryArgument(const std::string &name) const {
        auto &arguments = _request->getQueryArguments();
        return arguments.find(name) != arguments.end();
    }

    std::string getQueryArgument(const std::string &name, const char *defaultValue= nullptr, bool strip= true) const {
        return getArgument(name, _request->getQueryArguments(), defaultValue, strip);
    }

    StringVector getQueryArguments(const std::string &name, bool strip= true) const {
        return getArguments(name, _request->getQueryArguments(), strip);
    }

    const SimpleCookie& cookies() const {
        return _request->cookies();
    }

    std::string getCookie(const std::string &name, const std::string &defaultValue= "") const {
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

    void clearAllCookies(const char *path= "/", const char *domain= nullptr) {
        cookies().getAll([this, path, domain](const std::string &name, const Morsel &morsel) {
            clearCookie(name, path, domain);
        });
    }

    void redirect(const std::string &url, bool permanent=false, boost::optional<int> status=boost::none);

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

    void flush(bool includeFooters= false, FlushCallbackType callback= nullptr);

    template <typename... Args>
    void finish(Args&&... args) {
        write(std::forward<Args>(args)...);
        finish();
    }

    void finish();
    void sendError(int statusCode = 500, std::exception_ptr error= nullptr);
    virtual void writeError(int statusCode, std::exception_ptr error);
    void requireSetting(const std::string &name, const std::string &feature="this feature");

    template <typename... Args>
    std::string reverseURL(const std::string &name, Args&&... args);

    virtual boost::optional<std::string> computeEtag() const;

    void setEtagHeader() {
        auto etag = computeEtag();
        if (etag) {
            setHeader("Etag", *etag);
        }
    }

    bool checkEtagHeader() const {
        auto etag = _headers.get("Etag");
        std::string inm = _request->getHTTPHeaders()->get("If-None-Match");
        return !etag.empty() && !inm.empty() && inm.find(etag.c_str()) != std::string::npos;
    }

    std::shared_ptr<HTTPServerRequest> getRequest() const {
        return _request;
    }

    template <typename T>
    std::shared_ptr<T> getSelf() {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    template <typename T>
    std::shared_ptr<const T> getSelf() const {
        return std::static_pointer_cast<const T>(shared_from_this());
    }

    static const StringSet supportedMethods;
protected:
    std::string convertHeaderValue(const std::string &value) {
        if (value.length() > 4000 || boost::regex_search(value, _invalidHeaderCharRe)) {
            ThrowException(ValueError, "Unsafe header value " + value);
        }
        return value;
    }

    std::string convertHeaderValue(const char *value) {
        return convertHeaderValue(std::string(value));
    }

    std::string convertHeaderValue(const DateTime &value) {
        return convertHeaderValue(HTTPUtil::formatTimestamp(value));
    }

    template <typename T>
    std::string convertHeaderValue(T value) {
        return convertHeaderValue(std::to_string(value));
    }

    std::string getArgument(const std::string &name, const QueryArgListMap &source, const char *defaultValue= nullptr,
                            bool strip= true) const;

    StringVector getArguments(const std::string &name, const QueryArgListMap &source, bool strip= true) const;

    virtual void execute(TransformsType &transforms, StringVector args);

//    void whenComplete();

    void executeMethod();

    void executeFinish() {
        if (_autoFinish && !_finished) {
            finish();
        }
    }

    std::string generateHeaders() const;
    void log();

    std::string requestSummary() const {
        return _request->getMethod() + " " + _request->getURI() + " (" + _request->getRemoteIp() + ")";
    }

    void handleRequestException(std::exception_ptr error);

    virtual void logException(std::exception_ptr error);

    void clearHeadersFor304() {
        const StringVector headers = {"Allow", "Content-Encoding", "Content-Language",
                                      "Content-Length", "Content-MD5", "Content-Range",
                                      "Content-Type", "Last-Modified"};
        for (auto &h: headers) {
            clearHeader(h);
        }
    }

    Application *_application;
    std::shared_ptr<HTTPServerRequest> _request;
    bool _headersWritten{false};
    bool _finished{false};
    bool _autoFinish{true};
    TransformsType _transforms;
    StringVector _pathArgs;
    HTTPHeaders _headers;
//    ListHeadersType _listHeaders;
    ByteArray _writeBuffer;
    int _statusCode;
    CookiesType _newCookie;
    std::string _reason;

    static const boost::regex _removeControlCharsRegex;
    static const boost::regex _invalidHeaderCharRe;
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


#define Asynchronous()  this->_autoFinish = false; \
    ExceptionStackContext __ctx([this, self = shared_from_this()](std::exception_ptr error) { \
        handleRequestException(std::move(error)); \
    });
    

#define RemoveSlash()   { \
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
                this->redirect(uri, true); \
                return; \
            } \
        } else { \
            ThrowException(HTTPError, 404); \
        } \
    } \
}

#define AddSlash()  { \
    const std::string &path = this->_request->getPath(); \
    if (!boost::ends_with(path, "/")) { \
        const std::string &method = this->_request->getMethod(); \
        if (method == "GET" || method == "HEAD") { \
            std::string uri = path + "/"; \
            const std::string &query = this->_request->getQuery(); \
            if (!query.empty()) { \
                uri += "?" + query; \
            } \
            this->redirect(uri, true); \
            return; \
        } \
        ThrowException(HTTPError, 404); \
    } \
}


class TC_COMMON_API Application {
public:
    typedef PtrVector<URLSpec> HandlersType;
    typedef boost::regex HostPatternType;
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
    std::shared_ptr<HTTPServer> listen(unsigned short port, std::string address, Args&&... args) {
        std::shared_ptr<HTTPServer> server = std::make_shared<HTTPServer>(
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
    std::vector<URLSpec*> getHostHandlers(std::shared_ptr<HTTPServerRequest> request);

    TransformsType _transforms;
    HostHandlersType _handlers;
    NamedHandlersType _namedHandlers;
    std::string _defaultHost;
    SettingsType _settings;
};


#define HTTPServerCB(app) std::bind(&Application::operator(), pointer(app), std::placeholders::_1)


class TC_COMMON_API HTTPError: public Exception {
public:
    HTTPError(const char *file, int line, const char *func, int statusCode, const std::string &message="",
              const std::string &reason="")
            : Exception(file, line, func, message)
            , _statusCode(statusCode)
            , _reason(reason) {

    }

    const char *what() const noexcept override;

    const char *getTypeName() const override {
        return "HTTPError";
    }

    int getStatusCode() const {
        return _statusCode;
    }

    const std::string& getReason() const {
        return _reason;
    }
protected:
    int _statusCode;
    std::string _reason;
};


class TC_COMMON_API MissingArgumentError: public HTTPError {
public:
    MissingArgumentError(const char *file, int line, const char *func, const std::string &argName)
            : HTTPError(file, line, func, 400, "Missing argument " + argName)
            , _argName(argName) {

    }

    const char* getTypeName() const override {
        return "MissingArgumentError";
    }

    const std::string& getArgName() const {
        return _argName;
    }

protected:
    std::string _argName;
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
    void onGet(const StringVector &args) override;
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
    virtual void transformFirstChunk(int &statusCode, HTTPHeaders &headers, ByteArray &chunk, bool finishing) =0;
    virtual void transformChunk(ByteArray &chunk, bool finishing) =0;
};


class TC_COMMON_API GZipContentEncoding: public OutputTransform {
public:
    explicit GZipContentEncoding(std::shared_ptr<HTTPServerRequest> request);
    void transformFirstChunk(int &statusCode, HTTPHeaders &headers, ByteArray &chunk, bool finishing) override;
    void transformChunk(ByteArray &chunk, bool finishing) override;
protected:
    static const StringSet _contentTypes;
    static const int _minLength = 5;

    bool _gzipping;
    std::shared_ptr<std::stringstream> _gzipValue;
    GzipFile _gzipFile;
};


class TC_COMMON_API ChunkedTransferEncoding: public OutputTransform {
public:
    explicit ChunkedTransferEncoding(std::shared_ptr<HTTPServerRequest> request) {
        _chunking = request->supportsHTTP11();
    }

    void transformFirstChunk(int &statusCode, HTTPHeaders &headers, ByteArray &chunk, bool finishing) override;
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

    std::string toString() const {
        return String::format("URLSpec(%s, %s, name=%s)", _pattern.c_str(), _handlerClass.target_type().name(),
                              _name.c_str());
    }

    template <typename... Args>
    std::string reverse(Args&&... args) {
        ASSERT(!_path.empty(), "Cannot reverse url regex %s", _pattern.c_str());
        ASSERT(sizeof...(Args) == _groupCount, "required number of arguments not found");
        return formats(std::forward<Args>(args)...);
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
    template <typename... Args>
    std::string formats(Args&&... args) {
        boost::format formatter(_path);
        format(formatter, std::forward<Args>(args)...);
        return formatter.str();
    }

    template <typename... Args>
    void format(boost::format &formatter, const char *value, Args&&... args) {
        formatter % URLParse::quote(value);
        format(formatter, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void format(boost::format &formatter, const std::string &value, Args&&... args) {
        formatter % URLParse::quote(value);
        format(formatter, std::forward<Args>(args)...);
    }

    template <typename ValueT, typename... Args>
    void format(boost::format &formatter, ValueT &&value, Args&&... args) {
        formatter % URLParse::quote(std::to_string(value));
        format(formatter, std::forward<Args>(args)...);
    }

    void format(boost::format &formatter, const char *value) {
        formatter % URLParse::quote(value);
    }

    void format(boost::format &formatter, const std::string &value) {
        formatter % URLParse::quote(value);
    }

    template <typename ValueT>
    void format(boost::format &formatter, ValueT &&value) {
        formatter % URLParse::quote(std::to_string(value));
    }

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
