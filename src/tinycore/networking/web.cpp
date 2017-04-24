//
// Created by yuwenyong on 17-4-17.
//

#include "tinycore/networking/web.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "tinycore/debugging/trace.h"
#include "tinycore/logging/log.h"


RequestHandler::RequestHandler(Application *application, HTTPRequestPtr request)
        : _application(application)
        , _request(std::move(request)) {
    clear();
}

RequestHandler::~RequestHandler() {

}

void RequestHandler::start(ArgsType &args) {
    auto stream = _request->getConnection()->getStream();
    RequestHandlerWPtr handler = shared_from_this();
    stream->setCloseCallback([handler](){
        auto handlerKeeper = handler.lock();
        if (handlerKeeper) {
            handlerKeeper->onConnectionClose();
        }
    });
    initialize(args);
}

void RequestHandler::initialize(ArgsType &args) {

}

const RequestHandler::SettingsType& RequestHandler::getSettings() const {
    return _application->getSettings();
}

void RequestHandler::onHead(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::onGet(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::onPost(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::onDelete(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::onPut(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::onOptions(StringVector args) {
    ThrowException(HTTPError, 405);
}

void RequestHandler::prepare() {

}

void RequestHandler::onConnectionClose() {

}

void RequestHandler::clear() {
    _headers = {
            {"Server", TINYCORE_VER },
            {"Content-Type", "text/html; charset=UTF-8"},
    };
    if (!_request->supportsHTTP1_1()) {
        if (_request->getHTTPHeader()->getDefault("Connection") == "Keep-Alive") {
            setHeader("Connection", "Keep-Alive");
        }
    }
    _writeBuffer.reset();
    _statusCode = 200;
}

void RequestHandler::setHeader(const std::string &name, const std::string &value) {
    boost::regex unsafe(R"([\x00-\x1f])");
    if (value.length() > 4000 || boost::regex_search(value, unsafe)) {
        std::string error;
        error = "Unsafe header value " + value;
        ThrowException(ValueError, std::move(error));
    }
    _headers[name] = value;
}

void RequestHandler::setHeader(const std::string &name, const char *value) {
    boost::regex unsafe(R"([\x00-\x1f])");
    if (strlen(value) > 4000 || boost::regex_search(value, unsafe)) {
        std::string error;
        error = "Unsafe header value " + std::string(value);
        ThrowException(ValueError, std::move(error));
    }
    _headers[name] = value;
}

std::string RequestHandler::getArgument(const std::string &name, bool strip) {
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        std::string error;
        error = "Missing argument " + name;
        ThrowException(HTTPError, 404, std::move(error));
    }
    std::string value = iter->second.back();
    boost::regex contorlChars(R"([\x00-\x08\x0e-\x1f])");
    boost::regex_replace(value, contorlChars, " ");
    if (strip) {
        boost::trim(value);
    }
    return value;
}

std::string RequestHandler::getArgument(const std::string &name, const std::string &defualtValue, bool strip) {
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        return defualtValue;
    }
    std::string value = iter->second.back();
    boost::regex contorlChars(R"([\x00-\x08\x0e-\x1f])");
    boost::regex_replace(value, contorlChars, " ");
    if (strip) {
        boost::trim(value);
    }
    return value;
}

StringVector RequestHandler::getArguments(const std::string &name, bool strip) {
    StringVector values;
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        return values;
    }
    values = iter->second;
    boost::regex contorlChars(R"([\x00-\x08\x0e-\x1f])");
    for (auto &value: values) {
        boost::regex_replace(value, contorlChars, " ");
        if (strip) {
            boost::trim(value);
        }
    }
    return values;
}


Application::Application(HandlersType &handlers,
                         std::string defaultHost,
                         TransformsType transforms,
                         SettingsType settings)
        : _defaultHost(std::move(defaultHost))
        , _settings(std::move(settings)) {
    if (transforms.empty()) {
        if (_settings.find("gzip") != _settings.end() && boost::any_cast<bool>(_settings["gzip"])) {
            _transforms.push_back(OutputTransformFactory<GZipContentEncoding>());
        }
        _transforms.push_back(OutputTransformFactory<ChunkedTransferEncoding>());
    } else {
        _transforms = std::move(transforms);
    }
    if (!handlers.empty()) {
        addHandlers(".*$", handlers);
    }
}

Application::~Application() {

}

void Application::addHandlers(std::string hostPattern, HandlersType &hostHandlers) {
    if (!boost::ends_with(hostPattern, "$")) {
        hostPattern.push_back('$');
    }
    std::unique_ptr<HostHandlerType> handler = make_unique<HostHandlerType>();
    handler->first = HostPatternType::compile(hostPattern);
    HandlersType &handlers = handler->second;
    if (!_handlers.empty() && _handlers.back().second.front().getPattern() == ".*$") {
        auto iter = _handlers.end();
        std::advance(iter, -1);
        _handlers.insert(iter, handler.release());
    } else {
        _handlers.push_back(handler.release());
    }
    _handlers.transfer(_handlers.end(), hostHandlers);
    for (auto &spec: handlers) {
        if (!spec.getName().empty()) {
            if (_namedHandlers.find(spec.getName()) != _namedHandlers.end()) {
                Log::warn("Multiple handlers named %s; replacing previous value", spec.getName().c_str());
            }
            _namedHandlers[spec.getName()] = &spec;
        }
    }
}

void Application::operator()(HTTPRequestPtr request) {
    RequestHandler::TransformsType transforms;
    for (auto &transform: _transforms) {
        transforms.push_back(transform(request));
    }
    RequestHandlerPtr handler;
    StringVector args;
    auto handlers = getHostHandlers(request);
    if (!handlers) {
        RequestHandler::ArgsType handlerArgs = {
                {"url", "http://" + _defaultHost + "/"}
        };
        handler = RequestHandlerFactory<RedirectHandler>()(this, std::move(request), handlerArgs);
    } else {
        const std::string &requestPath = request->getPath();
        boost::xpressive::smatch match;
        for (auto &spec: *handlers) {
            if (boost::xpressive::regex_match(requestPath, match, spec.getRegex())) {
                handler = spec.getHandlerClass()(this, std::move(request), spec.getArgs());
                for (size_t i = 0; i < match.size(); ++i) {
                    args.push_back(match[i].str());
                }
                for (auto &s: args) {
                    s = URLParse::unquote(s);
                }
                break;
            }
        }
        if (!handler) {
            RequestHandler::ArgsType handlerArgs = {
                    {"statusCode", 404}
            };
            handler = RequestHandlerFactory<ErrorHandler>()(this, std::move(request), handlerArgs);
        }
    }
    handler->execute(transforms, std::move(args));
}

void Application::logRequest(RequestHandler *handler) const {
    auto iter = _settings.find("logFunction");
    if (iter != _settings.end()) {
        const LogFunctionType &logFunction = boost::any_cast<const LogFunctionType&>(iter->second);
        logFunction(handler);
        return;
    }
    int statusCode = handler->getStatus();
    std::string summary = handler->requestSummary();
    float requestTime = 1000.0f * handler->_request->requestTime();
    std::string logInfo = String::format("%d %s %.2fms", statusCode, summary.c_str(), requestTime);
    if (statusCode < 400) {
        Log::info(logInfo.c_str());
    } else if (statusCode < 500) {
        Log::warn(logInfo.c_str());
    } else {
        Log::error(logInfo.c_str());
    }
}

Application::HandlersType Application::defaultHandlers = {};

Application::HandlersType * Application::getHostHandlers(HTTPRequestPtr request) {
    std::string host = request->getHost();
    boost::to_lower(host);
    auto pos = host.find(':');
    if (pos != std::string::npos) {
        host = host.substr(0, pos);
    }
    for (auto &handler: _handlers) {
        if (boost::xpressive::regex_match(host, handler.first)) {
            return &handler.second;
        }
    }
    if (!request->getHTTPHeader()->contain("X-Real-Ip")) {
        for (auto &handler: _handlers) {
            if (boost::xpressive::regex_match(_defaultHost, handler.first)) {
                return &handler.second;
            }
        }
    }
    return nullptr;
}


const char* HTTPError::what() const noexcept {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += getTypeName();
        _what += "\n\t";
        _what += "HTTP ";
        _what += std::to_string(_statusCode);
        _what += " ";
        ASSERT(HTTPResponses.find(_statusCode) != HTTPResponses.end());
        _what += HTTPResponses[_statusCode];
        std::string what = std::runtime_error::what();
        if (!what.empty()) {
            _what += " (";
            _what += what;
            _what += ")";
        }
    }
    return _what.c_str();
}


void ErrorHandler::initialize(ArgsType &args) {
    setStatus(boost::any_cast<int>(args.at("statusCode")));
}

void ErrorHandler::prepare() {
    ThrowException(HTTPError, _statusCode);
}


void RedirectHandler::initialize(ArgsType &args) {
    _url = boost::any_cast<const std::string&>(args.at("url"));
    auto iter = args.find("permanent");
    if (iter != args.end()) {
        _permanent = boost::any_cast<bool>(iter->second);
    }
}

void RedirectHandler::onGet(StringVector args) {
    redirect(_url, _permanent);
}


void FallbackHandler::initialize(ArgsType &args) {
    _fallback = boost::any_cast<FallbackType&>(args.at("fallback"));
}

void FallbackHandler::prepare() {
    _fallback(_request);
    _finished = true;
}


GZipContentEncoding::GZipContentEncoding(HTTPRequestPtr request)
        : _gzipFile(_gzipValue) {
    if (request->supportsHTTP1_1()) {
        _gzipping = true;
    } else {
        auto header = request->getHTTPHeader();
        std::string acceptEncoding = header->getDefault("Accept-Encoding");
        _gzipping = acceptEncoding.find("gzip") != std::string::npos;
    }
}

void GZipContentEncoding::transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) {
    if (_gzipping) {
        auto iter = headers.find("Content-Type");
        std::string contentType = iter != headers.end() ? iter->second : "";
        auto pos = contentType.find(';');
        if (pos == std::string::npos) {
            ThrowException(IndexError, "list index out of range");
        }
        std::string ctype = contentType.substr(0, pos);
        _gzipping = CONTENT_TYPES.find(ctype) != CONTENT_TYPES.end()
                    && (!finishing || chunk.size() >= MIN_LENGTH)
                    && (finishing || headers.find("Content-Length") == headers.end())
                    && headers.find("Content-Encoding") == headers.end();
    }
    if (_gzipping) {
        headers["Content-Encoding"] = "gzip";
    }
}

void GZipContentEncoding::transformChunk(std::vector<char> &chunk, bool finishing) {
    if (_gzipping) {
        _gzipFile.write(chunk);
        if (finishing) {
            _gzipFile.close();
        } else {
            _gzipFile.flush();
        }
        chunk.assign(std::next(_gzipValue.begin(), _gzipPos), _gzipValue.end());
        _gzipPos = _gzipValue.size();
    }
}

const StringSet GZipContentEncoding::CONTENT_TYPES = {
        "text/plain",
        "text/html",
        "text/css",
        "text/xml",
        "application/x-javascript",
        "application/xml",
        "application/atom+xml",
        "text/javascript",
        "application/json",
        "application/xhtml+xml"
};


void ChunkedTransferEncoding::transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) {
    if (_chunking) {
        if (headers.find("Content-Length") != headers.end() || headers.find("Transfer-Encoding") != headers.end()) {
            _chunking = false;
        } else {
            headers["Transfer-Encoding"] = "chunked";
            transformChunk(chunk, finishing);
        }
    }
}

void ChunkedTransferEncoding::transformChunk(std::vector<char> &chunk, bool finishing) {
    if (_chunking) {
        std::string block;
        if (!chunk.empty()) {
            block = String::format("%x\r\n");
            chunk.insert(chunk.begin(), block.begin(), block.end());
            block = "\r\n";
            chunk.insert(chunk.end(), block.begin(), block.end());
        }
        if (finishing) {
            block = "0\r\n\r\n";
            chunk.insert(chunk.end(), block.begin(), block.end());
        }
    }
}


URLSpec::URLSpec(std::string pattern, HandlerClassType handlerClass, ArgsType args, std::string name)
        : _pattern(std::move(pattern))
        , _handlerClass(std::move(handlerClass))
        , _args(std::move(args))
        , _name(std::move(name)) {
    if (!boost::ends_with(_pattern, "$")) {
        _pattern.push_back('$');
    }
    _regex = RegexType::compile(_pattern);
    std::tie(_path, _groupCount) = findGroups();
}

std::tuple<std::string, int> URLSpec::findGroups() {
    auto beg = _pattern.begin(), end = _pattern.end();
    std::string pattern;
    if (boost::starts_with(pattern, "^")) {
        std::advance(beg, 1);
    }
    if (boost::ends_with(pattern, "$")) {
        std::advance(end, -1);
    }
    pattern.assign(beg, end);
    if (_regex.mark_count() != String::count(pattern, '(')) {
        return std::make_tuple("", -1);
    }
    StringVector pieces, fragments;
    std::string::size_type parenLoc;
    fragments = String::split(pattern, '(');
    for (auto &fragment: fragments) {
        parenLoc = fragment.find('(');
        if (parenLoc != std::string::npos) {
            pieces.push_back("%s" + fragment.substr(parenLoc + 1));
        } else {
            pieces.push_back(std::move(fragment));
        }
    }
    return std::make_tuple(boost::join(pieces, ""), _regex.mark_count());
}

