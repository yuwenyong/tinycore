//
// Created by yuwenyong on 17-5-22.
//

#define BOOST_TEST_MODULE web_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_TEST_DONT_PRINT_LOG_VALUE(StringVector)
BOOST_TEST_DONT_PRINT_LOG_VALUE(rapidjson::Document)
BOOST_TEST_DONT_PRINT_LOG_VALUE(Time)

using namespace rapidjson;

class SetCookieHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setCookie("str", "asdf");
        setCookie("unicode", "qwer");
        setCookie("bytes", "zxcv");
    }
};


class GetCookieHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        write(getCookie("foo", "default"));
    }
};


class SetCookieDomainHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setCookie("unicode_args", "blah", "foo.com", nullptr, "/foo");
    }
};


class SetCookieSpecialCharHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setCookie("equals", "a=b");
        setCookie("semicolon", "a;b");
        setCookie("quote", "a\"b");
    }
};


class SetCookieOverwriteHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setCookie("a", "b", "example.com");
        setCookie("c", "d", "example.com");
        setCookie("a", "e");
    }
};


class CookieTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<SetCookieHandler>("/set"),
                url<GetCookieHandler>("/get"),
                url<SetCookieDomainHandler>("/set_domain"),
                url<SetCookieSpecialCharHandler>("/special_char"),
                url<SetCookieOverwriteHandler>("/set_overwrite"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testSetCookie() {
        do {
            HTTPResponse response = fetch("/set");
            StringVector headers = response.getHeaders().getList("Set-Cookie");
            std::sort(headers.begin(), headers.end());
            StringVector cookies{"bytes=zxcv; Path=/", "str=asdf; Path=/", "unicode=qwer; Path=/"};
            BOOST_CHECK_EQUAL(headers, cookies);
        } while (false);
    }

    void testGetCookie() {
        do {
            HTTPHeaders headers{{"Cookie", "foo=bar"}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "bar");
        } while (false);

        do {
            HTTPHeaders headers{{"Cookie", "foo=\"bar\""}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "bar");
        } while (false);

        do {
            HTTPHeaders headers{{"Cookie", "/=exception;"}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "default");
        } while (false);
    }

    void testSetCookieDomain() {
        do {
            HTTPResponse response = fetch("/set_domain");
            StringVector cookies{"unicode_args=blah; Domain=foo.com; Path=/foo"};
            BOOST_CHECK_EQUAL(response.getHeaders().getList("Set-Cookie"), cookies);
        } while (false);
    }

    void testCookieSpecialChar() {
        do {
            HTTPResponse response = fetch("/special_char");
            auto headers = response.getHeaders().getList("Set-Cookie");
            std::sort(headers.begin(), headers.end());
            BOOST_CHECK_EQUAL(headers.size(), 3);
            BOOST_CHECK_EQUAL(headers[0], R"(equals="a=b"; Path=/)");
            BOOST_CHECK_EQUAL(headers[1], R"(quote="a\"b"; Path=/)");
            BOOST_CHECK_EQUAL(headers[2], R"(semicolon="a\073b"; Path=/)");
        } while (false);

        StringMap data = {{R"(foo=a=b)", "a=b"},
                          {R"(foo="a=b")", "a=b"},
                          {R"(foo="a;b")", "a;b"},
                          {R"(foo="a\073b")", "a;b"},
                          {R"(foo="a\"b")", "a\"b"}};
        for (auto &kv: data) {
            LOG_DEBUG("trying %s", kv.first.c_str());
            HTTPHeaders requestHeaders{{"Cookie", kv.first}};
            HTTPResponse response = fetch("/get", ARG_headers=requestHeaders);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, kv.second);
        }
    }

    void testSetCookieOverwrite() {
        do {
            HTTPResponse response = fetch("/set_overwrite");
            auto headers = response.getHeaders().getList("Set-Cookie");
            std::sort(headers.begin(), headers.end());
            StringVector cookies{"a=e; Path=/", "c=d; Domain=example.com; Path=/"};
            BOOST_CHECK_EQUAL(headers, cookies);
        } while (false);
    }
};

class ConnectionCloseTest;

class ConnectionCloseHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override {
        _test = boost::any_cast<ConnectionCloseTest *>(args["test"]);
    }

    void onGet(const StringVector &args) override;
    void onConnectionClose() override;
protected:
    ConnectionCloseTest *_test;
};


class ConnectionCloseTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        RequestHandler::ArgsType args = {
                {"test", const_cast<ConnectionCloseTest *>(this) }
        };

        Application::HandlersType handlers = {
                url<ConnectionCloseHandler>("/", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testConnectionClose() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->connect("localhost", getHTTPPort(), [this]() {
            stop();
        });
        wait();
        const char *data = "GET / HTTP/1.0\r\n\r\n";
        _stream->write((const Byte *)data, strlen(data));
        wait();
    }

    void onHandlerWaiting() {
        LOG_DEBUG("handler waiting");
        _stream->close();
    }

    void onConnectionClose() {
        LOG_DEBUG("connection closed");
        stop();
    }
protected:
    std::shared_ptr<IOStream> _stream;
};


void ConnectionCloseHandler::onGet(const StringVector &args) {
    Asynchronous()
    _test->onHandlerWaiting();
}

void ConnectionCloseHandler::onConnectionClose() {
    _test->onConnectionClose();
}


class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        Document doc;
        Document::AllocatorType &a = doc.GetAllocator();
        doc.SetObject();

        Value path(_request->getPath().c_str(), a);
        doc.AddMember(StringRef("path"), path, a);

        Value pathArgs;
        pathArgs.SetArray();
        for (auto &val: args) {
            pathArgs.PushBack(StringRef(val.c_str(), val.size()), a);
        }
        doc.AddMember(StringRef("pathArgs"), pathArgs, a);

        Value arguments;
        arguments.SetObject();
        for (auto &arg: _request->getArguments()) {
            rapidjson::Value values;
            values.SetArray();
            for (auto &val: arg.second) {
                values.PushBack(StringRef(val.c_str(), val.size()), a);
            }
            arguments.AddMember(StringRef(arg.first.c_str(), arg.first.size()), values, a);
        }
        doc.AddMember(StringRef("args"), arguments, a);
        write(doc);
    }
};


class RequestEncodingTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>(R"(/group/(.*))"),
                url<EchoHandler>(R"(/slashes/([^/]*)/([^/]*))"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    Document fetchJSON(const std::string path) {
        Document json;
        HTTPResponse response = fetch(path);
        const std::string *body = response.getBody();
        if (body) {
            json.Parse(body->c_str());
        }
//        std::cerr << String::fromJSON(json) << std::endl;
        return json;
    }

    void testQuestionMark() {
        using namespace rapidjson;
        do {
            Document json = fetchJSON("/group/%3F");

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            doc.AddMember(StringRef("path"), StringRef("/group/%3F"), a);

            Value pathArgs;
            pathArgs.SetArray();
            pathArgs.PushBack(StringRef("?"), a);
            doc.AddMember(StringRef("pathArgs"), pathArgs, a);

            Value arguments;
            arguments.SetObject();
            doc.AddMember(StringRef("args"), arguments, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);

        do {
            Document json = fetchJSON("/group/%3F?%3F=%3F");

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            doc.AddMember(StringRef("path"), StringRef("/group/%3F"), a);

            Value pathArgs;
            pathArgs.SetArray();
            pathArgs.PushBack(StringRef("?"), a);
            doc.AddMember(StringRef("pathArgs"), pathArgs, a);

            Value arguments;
            arguments.SetObject();
            Value values;
            values.SetArray();
            values.PushBack(StringRef("?"), a);
            arguments.AddMember(StringRef("?"), values, a);
            doc.AddMember(StringRef("args"), arguments, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);
    }

    void testSlashes() {
        do {
            Document json = fetchJSON("/slashes/foo/bar");

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            doc.AddMember(StringRef("path"), StringRef("/slashes/foo/bar"), a);

            Value pathArgs;
            pathArgs.SetArray();
            pathArgs.PushBack(StringRef("foo"), a);
            pathArgs.PushBack(StringRef("bar"), a);
            doc.AddMember(StringRef("pathArgs"), pathArgs, a);

            Value arguments;
            arguments.SetObject();
            doc.AddMember(StringRef("args"), arguments, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);

        do {
            Document json = fetchJSON("/slashes/a%2Fb/c%2Fd");

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            doc.AddMember(StringRef("path"), StringRef("/slashes/a%2Fb/c%2Fd"), a);

            Value pathArgs;
            pathArgs.SetArray();
            pathArgs.PushBack(StringRef("a/b"), a);
            pathArgs.PushBack(StringRef("c/d"), a);
            doc.AddMember(StringRef("pathArgs"), pathArgs, a);

            Value arguments;
            arguments.SetObject();
            doc.AddMember(StringRef("args"), arguments, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);
    }
};


class OptionalPathHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        std::string path;
        if (!args.empty()) {
            path = args[0];
        }
        Document doc;
        Document::AllocatorType &a = doc.GetAllocator();
        doc.SetObject();
        doc.AddMember(StringRef("path"), StringRef(path.c_str()), a);
        write(doc);
    }
};


class FlowControlHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        Asynchronous()
        write("1");
        flush(false, std::bind(&FlowControlHandler::step2, this));
    }

    void step2() {
        write("2");
        flush(false, std::bind(&FlowControlHandler::step3, this));
    }

    void step3() {
        write("3");
        finish();
    }
};


class MultiHeaderHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setHeader("x-overwrite", "1");
        setHeader("X-Overwrite", 2);
        addHeader("x-multi", 3);
        addHeader("X-Multi", "4");
    }
};


class _RedirectHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        std::string permanent, status;
        permanent = getArgument("permanent", "");
        status = getArgument("status", "");
        if (!permanent.empty()) {
            redirect("/", std::stoi(permanent) != 0);
        } else if (!status.empty()) {
            redirect("/", false, std::stoi(status));
        } else {
            ThrowException(Exception, "didn't get permanent or status arguments");
        }
    }
};


class EmptyFlushCallbackHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        Asynchronous()
        flush(false, [this]() {
            step2();
        });
    }

    void step2() {
        flush(false, [this]() {
            step3();
        });
    }

    void step3() {
        write("o");
        flush(false, [this]() {
            step4();
        });
    }

    void step4() {
        flush(false, [this]() {
           step5();
        });
    }

    void step5() {
        finish("k");
    }
};


class HeaderInjectHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        try {
            setHeader("X-Foo", "foo\r\nX-Bar: baz");
            ThrowException(Exception, "Didn't get expected exception");
        } catch (ValueError &e) {
            if (boost::contains(e.what(), "Unsafe header value")) {
                finish("ok");
            } else {
                throw;
            }
        }
    }
};


class GetArgumentHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        write(getArgument("foo", "default"));
    }
};


class WebTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<OptionalPathHandler>(R"(/optional_path/(.+)?)", "optional_path"),
                url<MultiHeaderHandler>("/multi_header"),
                url<_RedirectHandler>("/redirect"),
                url<HeaderInjectHandler>("/header_injection"),
                url<GetArgumentHandler>("/get_argument"),
                url<FlowControlHandler>("/flow_control"),
                url<EmptyFlushCallbackHandler>("/empty_flush"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    template <typename ...Args>
    Document fetchJSON(const std::string &path, Args&& ...args) {
        HTTPResponse response = fetch(path, std::forward<Args>(args)...);
        response.rethrow();
        const std::string *body = response.getBody();
        BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
        Document json;
        json.Parse(body->c_str(), body->size());
        return json;
    }

    void testReverseURL() {
        BOOST_CHECK_EQUAL(_app->reverseURL("optional_path", "foo"), "/optional_path/foo?");
//        std::cerr << _app->reverseURL("optional_path", 42) << std::endl;
        BOOST_CHECK_EQUAL(_app->reverseURL("optional_path", 42), "/optional_path/42?");
        BOOST_CHECK_EQUAL(_app->reverseURL("optional_path", "1 + 1"), "/optional_path/1%20%2B%201?");
    }

    void testOptionalPath() {
        do {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember(StringRef("path"), StringRef("foo"), a);
            BOOST_CHECK_EQUAL(fetchJSON("/optional_path/foo"), doc);
        } while (false);
        do {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember(StringRef("path"), StringRef(""), a);
            BOOST_CHECK_EQUAL(fetchJSON("/optional_path/"), doc);
        } while (false);
    }



    void testMultiHeader() {
        do {
            HTTPResponse response = fetch("/multi_header");
            auto &headers = response.getHeaders();
            BOOST_CHECK_EQUAL(headers.at("x-overwrite"), "2");
            StringVector rhs{"3", "4"};
            BOOST_CHECK_EQUAL(headers.getList("x-multi"), rhs);
        } while (false);
    }

    void testRedirect() {
        do {
            HTTPResponse response = fetch("/redirect?permanent=1", ARG_followRedirects=false);
            BOOST_CHECK_EQUAL(response.getCode(), 301);
        } while (false);

        do {
            HTTPResponse response = fetch("/redirect?permanent=0", ARG_followRedirects=false);
            BOOST_CHECK_EQUAL(response.getCode(), 302);
        } while (false);

        do {
            HTTPResponse response = fetch("/redirect?status=307", ARG_followRedirects=false);
            BOOST_CHECK_EQUAL(response.getCode(), 307);
        } while (false);
    }

    void testHeaderInject() {
        do {
            HTTPResponse response = fetch("/header_injection");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);
    }

    void testGetArgument() {
        do {
            HTTPResponse response = fetch("/get_argument?foo=bar");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "bar");
        } while (false);

        do {
            HTTPResponse response = fetch("/get_argument?foo=");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "");
        } while (false);

        do {
            HTTPResponse response = fetch("/get_argument");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "default");
        } while (false);
    }

    void testNoGzip() {
        do {
            HTTPResponse response = fetch("/get_argument");
            std::string vary = response.getHeaders().get("Vary", "");
//            std::cerr << "Vary:" << vary << std::endl;
            BOOST_CHECK(!boost::contains(vary, "Accept-Encoding"));
            std::string contentEncoding = response.getHeaders().get("Content-Encoding", "");
            BOOST_CHECK(!boost::contains(contentEncoding, "gzip"));
        } while (false);
    }

    void testFlowControl() {
        do {
            HTTPResponse response = fetch("/flow_control");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "123");
        } while (false);
    }

    void testEmptyFlush() {
        do {
            HTTPResponse response = fetch("/empty_flush");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);
    }
};


class DefaultHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        std::string status = getArgument("status", "");
        if (!status.empty()) {
            ThrowException(HTTPError, std::stoi(status));
        } else {
            ThrowException(ZeroDivisionError, "integer division or modulo by zero");
        }
    }
};


class WriteErrorHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        std::string status = getArgument("status", "");
        if (!status.empty()) {
            sendError(std::stoi(status));
        } else {
            ThrowException(ZeroDivisionError, "integer division or modulo by zero");
        }
    }

    void writeError(int statusCode, std::exception_ptr error) override {
        setHeader("Content-Type", "text/plain");
        if (error) {
            try {
                std::rethrow_exception(error);
            } catch (Exception &e) {
                write(String::format("Exception: %s", e.what()));
            }
        } else {
            write(String::format("Status: %d", statusCode));
        }
    }
};


class FailedWriteErrorHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        ThrowException(ZeroDivisionError, "integer division or modulo by zero");
    }

    void writeError(int statusCode, std::exception_ptr error) override {
        ThrowException(Exception, "exception in write_error");
    }
};


class ErrorResponseTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<DefaultHandler>("/default"),
                url<WriteErrorHandler>("/write_error"),
                url<FailedWriteErrorHandler>("/failed_write_error"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testDefault() {
        do {
            HTTPResponse response = fetch("/default");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::contains(*body, "500: Internal Server Error"));
        } while (false);

        do {
            HTTPResponse response = fetch("/default?status=503");
            BOOST_CHECK_EQUAL(response.getCode(), 503);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::contains(*body, "503: Service Unavailable"));
        } while (false);
    }

    void testWriteError() {
        do {
            HTTPResponse response = fetch("/write_error");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::contains(*body, "ZeroDivisionError"));
        } while (false);

        do {
            HTTPResponse response = fetch("/write_error?status=503");
            BOOST_CHECK_EQUAL(response.getCode(), 503);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Status: 503");
        } while (false);
    }

    void testFailedWriteError() {
        do {
            HTTPResponse response = fetch("/failed_write_error");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "");
        } while (false);
    }
};


class HostMatchingTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void initialize(ArgsType &args) override {
            _reply = boost::any_cast<std::string>(args["reply"]);
        }

        void onGet(const StringVector &args) override {
            write(_reply);
        }
    protected:
        std::string _reply;
    };

    std::unique_ptr<Application> getApp() const override {
        RequestHandler::ArgsType args = {
                {"reply", std::string("wildcard") }
        };
        Application::HandlersType handlers = {
                url<Handler>("/foo", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testHostMatching() {
        RequestHandler::ArgsType args1 = {
                {"reply", std::string("[0]") }
        };
        Application::HandlersType handlers1 = {
                url<Handler>("/foo", args1),
        };
        _app->addHandlers("www.example.com", std::move(handlers1));

        RequestHandler::ArgsType args2 = {
                {"reply", std::string("[1]") }
        };
        Application::HandlersType handlers2 = {
                url<Handler>("/bar", args2),
        };
        _app->addHandlers(R"(www\.example\.com)", std::move(handlers2));

        RequestHandler::ArgsType args3 = {
                {"reply", std::string("[2]") }
        };
        Application::HandlersType handlers3 = {
                url<Handler>("/baz", args3),
        };
        _app->addHandlers("www.example.com", std::move(handlers3));

        do {
            HTTPResponse response = fetch("/foo");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "wildcard");
        } while (false);

        do {
            HTTPResponse response = fetch("/bar");
            BOOST_CHECK_EQUAL(response.getCode(), 404);
        } while (false);

        do {
            HTTPResponse response = fetch("/baz");
            BOOST_CHECK_EQUAL(response.getCode(), 404);
        } while (false);

        do {
            HTTPHeaders headers{{"Host", "www.example.com"}};
            HTTPResponse response = fetch("/foo", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "[0]");
        } while (false);

        do {
            HTTPHeaders headers{{"Host", "www.example.com"}};
            HTTPResponse response = fetch("/bar", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "[1]");
        } while (false);

        do {
            HTTPHeaders headers{{"Host", "www.example.com"}};
            HTTPResponse response = fetch("/baz", ARG_headers=headers);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "[2]");
        } while (false);
    }
};


class ClearHeaderTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            setHeader("h1", "foo");
            setHeader("h2", "bar");
            clearHeader("h1");
            clearHeader("nonexistent");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        std::string defaultHost;
        Application::TransformsType transforms;
//        Application::LogFunctionType logFunction = [](RequestHandler *) {};
//        Application::SettingsType settings = {
//                {"logFunction", std::move(logFunction)},
//        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms));
    }

    void testClearHeader() {
        do {
            HTTPResponse response = fetch("/");
            BOOST_CHECK(!response.getHeaders().has("h1"));
            BOOST_CHECK_EQUAL(response.getHeaders().at("h2"), "bar");
        } while (false);
    }
};


class Header304Test: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            setHeader("Content-Language", "en_US");
            write("hello");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        std::string defaultHost;
        Application::TransformsType transforms;
//        Application::LogFunctionType logFunction = [](RequestHandler *) {};
//        Application::SettingsType settings = {
//                {"logFunction", std::move(logFunction)},
//        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms));
    }

    void test304Headers() {
        HTTPResponse response1 = fetch("/");
        BOOST_CHECK_EQUAL(response1.getHeaders().at("Content-Length"), "5");
        BOOST_CHECK_EQUAL(response1.getHeaders().at("Content-Language"), "en_US");

        HTTPHeaders headers;
        headers["If-None-Match"] = response1.getHeaders().at("Etag");
        HTTPResponse response2 = fetch("/", ARG_headers=headers);
        BOOST_CHECK_EQUAL(response2.getCode(), 304);
        BOOST_CHECK(!response2.getHeaders().has("Content-Length"));
        BOOST_CHECK(!response2.getHeaders().has("Content-Language"));
        BOOST_CHECK(!response2.getHeaders().has("Transfer-Encoding"));
    }
};


class StatusReasonTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            StringVector reason;
            if (_request->getArguments().find("reason") != _request->getArguments().end()) {
                reason = _request->getArguments().at("reason");
            }
            if (reason.empty()) {
                setStatus(std::stoi(getArgument("code")));
            } else {
                setStatus(std::stoi(getArgument("code")), reason[0]);
            }
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testStatus() {
        do {
            HTTPResponse response = fetch("/?code=304");
            BOOST_CHECK_EQUAL(response.getCode(), 304);
            BOOST_CHECK_EQUAL(response.getReason(), "Not Modified");
        } while (false);

        do {
            HTTPResponse response = fetch("/?code=304&reason=Foo");
            BOOST_CHECK_EQUAL(response.getCode(), 304);
            BOOST_CHECK_EQUAL(response.getReason(), "Foo");
        } while (false);

        do {
            HTTPResponse response = fetch("/?code=682&reason=Bar");
            BOOST_CHECK_EQUAL(response.getCode(), 682);
            BOOST_CHECK_EQUAL(response.getReason(), "Bar");
        } while (false);

        do {
            HTTPResponse response = fetch("/?code=682");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
        } while (false);
    }
};


class DateHeaderTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            write("Hello");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testDateHeader() {
        do {
            HTTPResponse response = fetch("/");
            DateTime dt = String::parseUTCDate(response.getHeaders().at("Date"));
            DateTime now = boost::posix_time::second_clock::universal_time();
            BOOST_CHECK_LT(dt - now, boost::posix_time::seconds(2));
//            std::cerr << "SUCCESS\n";
        } while (false);
    }
};


class RaiseWithReasonTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            ThrowException(HTTPError, 682, "", "Foo");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testRaiseWithReason() {
        do {
            HTTPResponse response = fetch("/");
            BOOST_CHECK_EQUAL(response.getCode(), 682);
            BOOST_CHECK_EQUAL(response.getReason(), "Foo");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
//            std::cerr << ">>" << *body << std::endl;
            BOOST_CHECK(boost::contains(*body, "682: Foo"));
        } while (false);
    }
};


class GzipTestCase: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            if (hasArgument("vary")) {
                setHeader("Vary", getArgument("vary"));
            }
            write("hello world");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        std::string defaultHost;
        Application::TransformsType transforms;
        Application::SettingsType settings = {
                {"gzip", true},
        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms),
                                        std::move(settings));
    }

    void testGzip() {
        do {
            HTTPResponse response = fetch("/");
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-Encoding"), "gzip");
            BOOST_CHECK_EQUAL(response.getHeaders().at("Vary"), "Accept-Encoding");
        } while (false);
    }

    void testGzipNotRequested() {
        do {
            HTTPResponse response = fetch("/", ARG_useGzip=false);
            BOOST_CHECK(!response.getHeaders().has("Content-Encoding"));
            BOOST_CHECK_EQUAL(response.getHeaders().at("Vary"), "Accept-Encoding");
        } while (false);
    }

    void testVaryAlreadyPresent() {
        do {
            HTTPResponse response = fetch("/?vary=Accept-Language");
            BOOST_CHECK_EQUAL(response.getHeaders().at("Vary"), "Accept-Language, Accept-Encoding");
        } while (false);
    }
};


class PathArgsInPrepareTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void prepare() override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            Value pathArgs;
            pathArgs.SetArray();
            for (auto &val: _pathArgs) {
                pathArgs.PushBack(StringRef(val.c_str(), val.size()), a);
            }
            doc.AddMember(StringRef("args"), pathArgs, a);

            write(doc);
        }

        void onGet(const StringVector &args) override {
            ASSERT(args.size() == 1 && args[0] == "foo");
            finish();
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/pos/(.*)"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testPos() {
        do {
            HTTPResponse response = fetch("/pos/foo");
            response.rethrow();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));

            Document data;
            data.Parse(body->c_str(), body->size());

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            Value pathArgs;
            pathArgs.SetArray();
            pathArgs.PushBack(StringRef("foo"), a);
            doc.AddMember(StringRef("args"), pathArgs, a);
            BOOST_CHECK_EQUAL(data, doc);
        } while (false);
    }
};


class ClearAllCookiesTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
//            std::cerr << "Clear Cookies" << std::endl;
            clearAllCookies();
            write("ok");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testClearAllCookies() {
        do {
            HTTPHeaders headers = {{"Cookie", "foo=bar; baz=xyzzy"}};
            HTTPResponse response = fetch("/", ARG_headers=headers);
            auto setCookies = response.getHeaders().getList("Set-Cookie");
            std::sort(setCookies.begin(), setCookies.end());
//            std::cerr << "Cookies:" << setCookies.size() << std::endl;
            BOOST_CHECK(boost::starts_with(setCookies[0], "baz=;"));
            BOOST_CHECK(boost::starts_with(setCookies[1], "foo=;"));
        } while (false);
    }
};


class ExceptionHandlerTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            auto exc = getArgument("exc");
            if (exc == "http") {
                ThrowException(HTTPError, 410, "no longer here");
            } else if (exc == "zero") {
                ThrowException(ZeroDivisionError, "");
            } else if (exc == "permission") {
                ThrowException(PermissionError, "not allowed");
            }
        }

        void writeError(int statusCode, std::exception_ptr error) override {
            if (error) {
                try {
                    std::rethrow_exception(error);
                } catch (PermissionError &e) {
                    setStatus(403);
                    write("PermissionError");
                    return;
                } catch (...) {

                }
            }
            RequestHandler::writeError(statusCode, error);
        }

        void logException(std::exception_ptr error) override {
            if (error) {
                try {
                    std::rethrow_exception(error);
                } catch (PermissionError &e) {
                    LOG_WARNING(gAppLog, "custom logging for PermissionError: %s", e.what());
                    return;
                } catch (...) {

                }
            }
            RequestHandler::logException(error);
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testHTTPError() {
        do {
            HTTPResponse response = fetch("/?exc=http");
            BOOST_CHECK_EQUAL(response.getCode(), 410);
        } while (false);
    }

    void testUnknownError() {
        do {
            HTTPResponse response = fetch("/?exc=zero");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
        } while (false);
    }

    void testKnownError() {
        do {
            HTTPResponse response = fetch("/?exc=permission");
            BOOST_CHECK_EQUAL(response.getCode(), 403);
        } while (false);
    }
};


class GetArgumentErrorTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();

            try {
                getArgument("foo");
                write(doc);
            } catch (MissingArgumentError &e) {
                doc.AddMember(StringRef("argName"), StringRef(e.getArgName().c_str()), a);
                doc.AddMember(StringRef("logMessage"), StringRef(e.what()), a);
                write(doc);
            }
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testCatchError() {
        do {
            HTTPResponse response = fetch("/");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            std::cerr << *body << std::endl;
            Document data;
            data.Parse(body->c_str(), body->size());
            BOOST_CHECK(data["argName"] == "foo");
            BOOST_CHECK(boost::contains(data["logMessage"].GetString(), "Missing argument foo"));
        } while (false);
    }
};


class MultipleExceptionTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Asynchronous();
            IOLoop::current()->addCallback([]() {
                ThrowException(ZeroDivisionError, "");
            });
            IOLoop::current()->addCallback([]() {
                ThrowException(ZeroDivisionError, "");
            });
        }

        void logException(std::exception_ptr error) override {
            ++errorCount;
//            RequestHandler::logException(error);
        }

        static int errorCount;
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testMultiException() {
        do {
            HTTPResponse response = fetch("/");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
        } while (false);

        do {
            HTTPResponse response = fetch("/");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
        } while (false);

        BOOST_CHECK_GT(Handler::errorCount, 2);
        std::cerr << Handler::errorCount << std::endl;
    }
};

int MultipleExceptionTest::Handler::errorCount = 0;


class UnimplementedHTTPMethodsTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testUnimplementedStandardMethods() {
        for (auto method: {"HEAD", "GET", "DELETE", "OPTIONS"}) {
            HTTPResponse response = fetch("/", ARG_method=method);
            BOOST_CHECK_EQUAL(response.getCode(), 405);
        }

        for (auto method: {"POST", "PUT"}) {
            HTTPResponse response = fetch("/", ARG_method=method, ARG_body="");
            BOOST_CHECK_EQUAL(response.getCode(), 405);
        }
    }
};


class UnimplementedNonStandardMethodsTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testUnimplementedPatch() {
        do {
            HTTPResponse response = fetch("/", ARG_method="PATCH", ARG_body="");
            BOOST_CHECK_EQUAL(response.getCode(), 405);
        } while (false);
    }
};


class AllHTTPMethodsTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            onMethod();
        }

        void onPost(const StringVector &args) override {
            onMethod();
        }

        void onPut(const StringVector &args) override {
            onMethod();
        }

        void onDelete(const StringVector &args) override {
            onMethod();
        }

        void onOptions(const StringVector &args) override {
            onMethod();
        }

//        void onPatch(const StringVector &args) override {
//            onMethod();
//        }

        void onMethod() {
            write(_request->getMethod());
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testStandardMethods() {
        do {
            HTTPResponse response = fetch("/", ARG_method="HEAD");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "");
        } while (false);

        for (std::string method: {"GET", "DELETE", "OPTIONS"}) {
            HTTPResponse response = fetch("/", ARG_method=method);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, method);
        }

        for (std::string method: {"POST", "PUT"}) {
            HTTPResponse response = fetch("/", ARG_method=method, ARG_body="");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, method);
        }
    }
};


class PatchMethodTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onPatch(const StringVector &args) override {
            write("patch");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testPatch() {
        do {
            HTTPResponse response = fetch("/", ARG_method="PATCH", ARG_body="");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "patch");
        } while (false);
    }
};


class FinishInPrepareTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void prepare() override {
            finish("done");
        }

        void onGet(const StringVector &args) override {
            ThrowException(Exception, "should not reach this method");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testFinishInPrepare() {
        do {
            HTTPResponse response = fetch("/");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "done");
        } while (false);
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(CookieTest, testSetCookie)
TINYCORE_TEST_CASE(CookieTest, testGetCookie)
TINYCORE_TEST_CASE(CookieTest, testSetCookieDomain)
TINYCORE_TEST_CASE(CookieTest, testCookieSpecialChar)
TINYCORE_TEST_CASE(CookieTest, testSetCookieOverwrite)
TINYCORE_TEST_CASE(ConnectionCloseTest, testConnectionClose)
TINYCORE_TEST_CASE(RequestEncodingTest, testQuestionMark)
TINYCORE_TEST_CASE(RequestEncodingTest, testSlashes)
TINYCORE_TEST_CASE(WebTest, testReverseURL)
TINYCORE_TEST_CASE(WebTest, testOptionalPath)
TINYCORE_TEST_CASE(WebTest, testMultiHeader)
TINYCORE_TEST_CASE(WebTest, testRedirect)
TINYCORE_TEST_CASE(WebTest, testHeaderInject)
TINYCORE_TEST_CASE(WebTest, testGetArgument)
TINYCORE_TEST_CASE(WebTest, testNoGzip)
TINYCORE_TEST_CASE(WebTest, testFlowControl)
TINYCORE_TEST_CASE(WebTest, testEmptyFlush)
TINYCORE_TEST_CASE(ErrorResponseTest, testDefault)
TINYCORE_TEST_CASE(ErrorResponseTest, testWriteError)
TINYCORE_TEST_CASE(ErrorResponseTest, testFailedWriteError)
TINYCORE_TEST_CASE(HostMatchingTest, testHostMatching)
TINYCORE_TEST_CASE(ClearHeaderTest, testClearHeader)
TINYCORE_TEST_CASE(Header304Test, test304Headers)
TINYCORE_TEST_CASE(StatusReasonTest, testStatus)
TINYCORE_TEST_CASE(DateHeaderTest, testDateHeader)
TINYCORE_TEST_CASE(RaiseWithReasonTest, testRaiseWithReason)
TINYCORE_TEST_CASE(GzipTestCase, testGzip)
TINYCORE_TEST_CASE(GzipTestCase, testGzipNotRequested)
TINYCORE_TEST_CASE(GzipTestCase, testVaryAlreadyPresent)
TINYCORE_TEST_CASE(PathArgsInPrepareTest, testPos)
TINYCORE_TEST_CASE(ClearAllCookiesTest, testClearAllCookies)
TINYCORE_TEST_CASE(ExceptionHandlerTest, testHTTPError)
TINYCORE_TEST_CASE(ExceptionHandlerTest, testUnknownError)
TINYCORE_TEST_CASE(ExceptionHandlerTest, testKnownError)
TINYCORE_TEST_CASE(GetArgumentErrorTest, testCatchError)
TINYCORE_TEST_CASE(MultipleExceptionTest, testMultiException)
TINYCORE_TEST_CASE(UnimplementedHTTPMethodsTest, testUnimplementedStandardMethods)
TINYCORE_TEST_CASE(UnimplementedNonStandardMethodsTest, testUnimplementedPatch)
TINYCORE_TEST_CASE(AllHTTPMethodsTest, testStandardMethods)
TINYCORE_TEST_CASE(PatchMethodTest, testPatch)
TINYCORE_TEST_CASE(FinishInPrepareTest, testFinishInPrepare)
