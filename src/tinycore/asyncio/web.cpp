//
// Created by yuwenyong on 17-4-17.
//

#include "tinycore/asyncio/web.h"
#include <boost/regex.hpp>
#include "tinycore/crypto/hashlib.h"
#include "tinycore/debugging/trace.h"
#include "tinycore/debugging/watcher.h"
#include "tinycore/logging/log.h"


RequestHandler::RequestHandler(Application *application, std::shared_ptr<HTTPServerRequest> request)
        : _application(application)
        , _request(std::move(request)) {
    clear();
#ifndef NDEBUG
    sWatcher->inc(SYS_REQUESTHANDLER_COUNT);
#endif
}

RequestHandler::~RequestHandler() {
#ifndef NDEBUG
    sWatcher->dec(SYS_REQUESTHANDLER_COUNT);
#endif
}

void RequestHandler::start(ArgsType &args) {
    _request->getConnection()->getStream()->setCloseCallback(std::bind(&RequestHandler::onConnectionClose,
                                                                       shared_from_this()));
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
//    _listHeaders.clear();
    setDefaultHeaders();
    if (!_request->supportsHTTP1_1()) {
        if (_request->getHTTPHeaders()->get("Connection") == "Keep-Alive") {
            setHeader("Connection", "Keep-Alive");
        }
    }
//    _writeBuffer.clear();
    _statusCode = 200;
}

void RequestHandler::setDefaultHeaders() {

}

std::string RequestHandler::getArgument(const std::string &name, const char *defaultValue, bool strip) const {
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        if (defaultValue == nullptr) {
            std::string error = "Missing argument " + name;
            ThrowException(HTTPError, 400, std::move(error));
        }
        return std::string(defaultValue);
    }
    std::string value = iter->second.back();
    boost::regex controlChars(R"([\x00-\x08\x0e-\x1f])");
    boost::regex_replace(value, controlChars, " ");
    if (strip) {
        boost::trim(value);
    }
    return value;
}

std::string RequestHandler::getArgument(const std::string &name, const std::string &defaultValue, bool strip) const {
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        return defaultValue;
    }
    std::string value = iter->second.back();
    boost::regex controlChars(R"([\x00-\x08\x0e-\x1f])");
    boost::regex_replace(value, controlChars, " ");
    if (strip) {
        boost::trim(value);
    }
    return value;
}

StringVector RequestHandler::getArguments(const std::string &name, bool strip) const {
    StringVector values;
    auto &arguments = _request->getArguments();
    auto iter = arguments.find(name);
    if (iter == arguments.end()) {
        return values;
    }
    values = iter->second;
    boost::regex controlChars(R"([\x00-\x08\x0e-\x1f])");
    for (auto &value: values) {
        boost::regex_replace(value, controlChars, " ");
        if (strip) {
            boost::trim(value);
        }
    }
    return values;
}

void RequestHandler::setCookie(const std::string &name, const std::string &value, const char *domain,
                               const DateTime *expires, const char *path, int *expiresDays, const StringMap *args) {
    boost::regex patt(R"([\x00-\x20])");
    if (boost::regex_search(name + value, patt)) {
        ThrowException(ValueError, String::format("Invalid cookie %s: %s", name.c_str(), value.c_str()));
    }
    if (!_newCookies) {
        _newCookies.emplace();
    }
    _newCookies->emplace_back();
    SimpleCookie &newCookie = _newCookies->back();
    newCookie[name] = value;
    Morsel &morsel = newCookie.at(name);
    if (domain) {
        morsel["domain"] = domain;
    }
    DateTime temp;
    if (expiresDays && !expires) {
        temp = boost::posix_time::second_clock::universal_time() + boost::gregorian::days(*expiresDays);
        expires = &temp;
    }
    if (expires) {
        morsel["expires"] = String::formatUTCDate(*expires, true);
    }
    if (path) {
        morsel["path"] = path;
    }
    if (args) {
        for (auto &kv: *args) {
            morsel[kv.first] = kv.second;
        }
    }
}

void RequestHandler::redirect(const std::string &url, bool permanent) {
    if (_headersWritten) {
        ThrowException(Exception, "Cannot redirect after headers have been written");
    }
    setStatus(permanent ? 301 : 302);
    const boost::regex patt(R"([\x00-\x20]+)");
    std::string location = URLParse::urlJoin(_request->getURI(), boost::regex_replace(url, patt, ""));
    setHeader("Location", location);
    finish();
}

void RequestHandler::flush(bool includeFooters, FlushCallbackType callback) {
    ByteArray chunk = std::move(_writeBuffer);
    std::string headers;
    if (!_headersWritten) {
        _headersWritten = true;
        for (auto &transfrom: _transforms) {
            transfrom.transformFirstChunk(_headers, chunk, includeFooters);
        }
        headers = generateHeaders();
    } else {
        for (auto &transfrom: _transforms) {
            transfrom.transformChunk(chunk, includeFooters);
        }
    }
    if (_request->getMethod() == "HEAD") {
        if (!headers.empty()) {
            _request->write(headers, std::move(callback));
        }
        return;
    }
    if (!chunk.empty()) {
        headers.append((const char *)chunk.data(), chunk.size());
    }
    if (!headers.empty()) {
        _request->write(headers, std::move(callback));
    }
}

void RequestHandler::finish() {
    ASSERT(!_finished);
    if (!_headersWritten) {
        const std::string &method = _request->getMethod();
        if (_statusCode == 200 && (method == "GET" || method == "HEAD") && _headers.find("Etag") == _headers.end()) {
            std::string etag = computeEtag();
            if (!etag.empty()) {
                std::string inm = _request->getHTTPHeaders()->get("If-None-Match");
                if (inm.find(etag.c_str()) != std::string::npos) {
                    _writeBuffer.clear();
                    setStatus(304);
                } else {
                    setHeader("Etag", etag);
                }
            }
        }
        if (_headers.find("Content-Length") == _headers.end()) {
            setHeader("Content-Length", _writeBuffer.size());
        }
    }
    _request->getConnection()->getStream()->setCloseCallback(nullptr);
    flush(true);
    _request->finish();
    log();
    _finished = true;
}

void RequestHandler::sendError(int statusCode, std::exception_ptr error) {
    if (_headersWritten) {
        Log::error("Cannot send error response after headers written");
        if (!_finished) {
            finish();
            return;
        }
    }
    clear();
    setStatus(statusCode);
    try {
        writeError(statusCode, error);
    } catch (std::exception &e) {
        Log::error("Uncaught exception in writeError: %s", e.what());
    }
    if (!_finished) {
        finish();
    }
}

void RequestHandler::writeError(int statusCode, std::exception_ptr error) {
    const std::string &message = HTTPResponses.at(statusCode);
    finish(String::format("<html><title>%d: %s</title><body>%d: %s</body></html>", statusCode, message.c_str(),
                          statusCode, message.c_str()));
}

void RequestHandler::requireSetting(const std::string &name, const std::string &feature) {
    const Application::SettingsType &settings = _application->getSettings();
    if (settings.find("name") == settings.end()) {
        ThrowException(Exception, String::format("You must define the '%s' setting in your application to use %s",
                                                 name.c_str(), feature.c_str()));
    }
}

std::string RequestHandler::computeEtag() const {
    SHA1Object hasher;
    hasher.update(_writeBuffer.data(), _writeBuffer.size());
    std::string etag = "\"" + hasher.hex() + "\"";
    return etag;
}

const StringSet RequestHandler::supportedMethods = {
        "GET", "HEAD", "POST", "DELETE", "PUT", "OPTIONS"
};

void RequestHandler::execute(TransformsType &transforms, StringVector args) {
    _transforms.transfer(_transforms.end(), transforms);
    std::exception_ptr error;
    try {
        const std::string &method = _request->getMethod();
        if (supportedMethods.find(method) == supportedMethods.end()) {
            ThrowException(HTTPError, 405);
        }
        prepare();
        if (!_finished) {
            if (method == "HEAD") {
                onHead(std::move(args));
            } else if (method == "GET") {
                onGet(std::move(args));
            } else if (method == "POST") {
                onPost(std::move(args));
            } else if (method == "DELETE") {
                onDelete(std::move(args));
            } else if (method == "PUT") {
                onPut(std::move(args));
            } else {
                ASSERT(method == "OPTIONS");
                onOptions(std::move(args));
            }
            if (_autoFinish && !_finished) {
                finish();
            }
        }
    } catch (...) {
        error = std::current_exception();
    }
    if (error) {
        handleRequestException(error);
    }
}

std::string RequestHandler::generateHeaders() const {
    StringVector lines;
    lines.emplace_back(_request->getVersion() + " " + std::to_string(_statusCode) + " "
                       + HTTPResponses.at(_statusCode));
    for (auto &header: _headers) {
        lines.emplace_back(header.first + ": " + header.second);
    }
    for (auto &header: _listHeaders) {
        lines.emplace_back(header.first + ": " + header.second);
    }
    if (_newCookies) {
        for (auto &cookieDict: _newCookies.get()) {
            cookieDict.getAll([&lines](const std::string &key, const Morsel &cookie) {
                lines.emplace_back("Set-Cookie: " + cookie.outputString());
            });
        }
    }
    return boost::join(lines, "\r\n") + "\r\n\r\n";
}

void RequestHandler::log() {
    _application->logRequest(this);
}

void RequestHandler::handleRequestException(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (HTTPError &e) {
        std::string summary = requestSummary();
        int statusCode = e.getStatusCode();
        Log::warn("%d %s: %s", statusCode, summary.c_str(), e.what());
        if (HTTPResponses.find(statusCode) == HTTPResponses.end()) {
            Log::error("Bad HTTP status code: %d", statusCode);
            sendError(500, error);
        } else {
            sendError(statusCode, error);
        }
    } catch (std::exception &e) {
        std::string summary = requestSummary();
        std::string requestInfo = _request->dump();
        Log::error("Uncaught exception %s\n%s\n%s", e.what(), summary.c_str(), requestInfo.c_str());
        sendError(500, error);
    } catch (...) {
        std::string summary = requestSummary();
        std::string requestInfo = _request->dump();
        Log::error("Unknown exception\n%s\n%s", summary.c_str(), requestInfo.c_str());
        sendError(500, error);
    }
}


Application::Application(HandlersType &&handlers,
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
        addHandlers(".*$", std::move(handlers));
    }
}

Application::~Application() {

}

void Application::addHandlers(std::string hostPattern, HandlersType &&hostHandlers) {
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
    handlers.transfer(handlers.end(), hostHandlers);
    for (auto &spec: handlers) {
        if (!spec.getName().empty()) {
            if (_namedHandlers.find(spec.getName()) != _namedHandlers.end()) {
                Log::warn("Multiple handlers named %s; replacing previous value", spec.getName().c_str());
            }
            _namedHandlers[spec.getName()] = &spec;
        }
    }
}

void Application::operator()(std::shared_ptr<HTTPServerRequest> request) {
    RequestHandler::TransformsType transforms;
    for (auto &transform: _transforms) {
        transforms.push_back(transform(request));
    }
    std::shared_ptr<RequestHandler> handler;
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
                for (size_t i = 1; i < match.size(); ++i) {
                    args.push_back(match[i].str());
                }
                for (auto &s: args) {
                    s = URLParse::unquotePlus(s);
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

//Application::HandlersType Application::defaultHandlers = {};

Application::HandlersType * Application::getHostHandlers(std::shared_ptr<HTTPServerRequest> request) {
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
    if (!request->getHTTPHeaders()->has("X-Real-Ip")) {
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
        _what += ": ";
        ASSERT(HTTPResponses.find(_statusCode) != HTTPResponses.end());
        _what += HTTPResponses.at(_statusCode);
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


GZipContentEncoding::GZipContentEncoding(std::shared_ptr<HTTPServerRequest> request) {
    if (request->supportsHTTP1_1()) {
        auto headers = request->getHTTPHeaders();
        std::string acceptEncoding = headers->get("Accept-Encoding");
        _gzipping = acceptEncoding.find("gzip") != std::string::npos;
    } else {
        _gzipping = false;
    }
}

void GZipContentEncoding::transformFirstChunk(StringMap &headers, ByteArray &chunk, bool finishing) {
    if (_gzipping) {
        auto iter = headers.find("Content-Type");
        std::string ctype = iter != headers.end() ? iter->second : "";
        auto pos = ctype.find(';');
        if (pos != std::string::npos) {
            ctype = ctype.substr(0, pos);
        }
        _gzipping = _contentTypes.find(ctype) != _contentTypes.end()
                    && (!finishing || chunk.size() >= _minLength)
                    && (finishing || headers.find("Content-Length") == headers.end())
                    && headers.find("Content-Encoding") == headers.end();
    }
    if (_gzipping) {
        headers["Content-Encoding"] = "gzip";
        _gzipValue = std::make_shared<std::stringstream>();
        _gzipFile.initWithOutputStream(_gzipValue);
//        _gzipPos = 0;
        transformChunk(chunk, finishing);
        if (headers.find("Content-Length") != headers.end()) {
            headers["Content-Length"] = std::to_string(chunk.size());
        }
    }
}

void GZipContentEncoding::transformChunk(ByteArray &chunk, bool finishing) {
    if (_gzipping) {
        _gzipFile.write(chunk);
        if (finishing) {
            _gzipFile.close();
        } else {
            _gzipFile.flush();
        }
        auto length = _gzipValue->tellp() - _gzipValue->tellg();
        chunk.resize((size_t)length);
        _gzipValue->read((char *)chunk.data(), length);
//        std::string gzipValue = _gzipValue->str();
//        chunk.assign((const Byte *)gzipValue.data() + _gzipPos, (const Byte *)gzipValue.data() + gzipValue.size());
//        _gzipPos += chunk.size();
    }
}

const StringSet GZipContentEncoding::_contentTypes = {
        "text/plain",
        "text/html",
        "text/css",
        "text/xml",
        "application/javascript",
        "application/x-javascript",
        "application/xml",
        "application/atom+xml",
        "text/javascript",
        "application/json",
        "application/xhtml+xml"
};

const int GZipContentEncoding::_minLength;


void ChunkedTransferEncoding::transformFirstChunk(StringMap &headers, ByteArray &chunk, bool finishing) {
    if (_chunking) {
        if (headers.find("Content-Length") != headers.end() || headers.find("Transfer-Encoding") != headers.end()) {
            _chunking = false;
        } else {
            headers["Transfer-Encoding"] = "chunked";
            transformChunk(chunk, finishing);
        }
    }
}

void ChunkedTransferEncoding::transformChunk(ByteArray &chunk, bool finishing) {
    if (_chunking) {
        std::string block;
        if (!chunk.empty()) {
            block = String::format("%x\r\n", (int)chunk.size());
            chunk.insert(chunk.begin(), (const Byte *)block.data(), (const Byte *)block.data() + block.size());
            block = "\r\n";
            chunk.insert(chunk.end(), (const Byte *)block.data(), (const Byte *)block.data() + block.size());
        }
        if (finishing) {
            block = "0\r\n\r\n";
            chunk.insert(chunk.end(), (const Byte *)block.data(), (const Byte *)block.data() + block.size());
        }
    }
}


URLSpec::URLSpec(std::string pattern, HandlerClassType handlerClass, std::string name)
        : URLSpec(std::move(pattern), std::move(handlerClass), {}, std::move(name)) {

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
    if (boost::starts_with(_pattern, "^")) {
        std::advance(beg, 1);
    }
    if (boost::ends_with(_pattern, "$")) {
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
        parenLoc = fragment.find(')');
        if (parenLoc != std::string::npos) {
            pieces.push_back("%s" + fragment.substr(parenLoc + 1));
        } else {
            pieces.push_back(std::move(fragment));
        }
    }
    return std::make_tuple(boost::join(pieces, ""), _regex.mark_count());
}

