//
// Created by yuwenyong on 17-5-22.
//

#define BOOST_TEST_MODULE web_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_TEST_DONT_PRINT_LOG_VALUE(StringVector)
BOOST_TEST_DONT_PRINT_LOG_VALUE(rapidjson::Document)

using namespace rapidjson;

class SetCookieHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        setCookie("str", "asdf");
        setCookie("unicode", "qwer");
        setCookie("bytes", "zxcv");
    }
};


class GetCookieHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write(getCookie("foo", "default"));
    }
};


class SetCookieDomainHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        setCookie("unicode_args", "blah", "foo.com", nullptr, "/foo");
    }
};


class SetCookieSpecialCharHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        setCookie("equals", "a=b");
        setCookie("semicolon", "a;b");
        setCookie("quote", "a\"b");
    }
};


class SetCookieOverwriteHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
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
            LOG_INFO("trying %s", kv.first.c_str());
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

    void onGet(StringVector args) override;
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
        LOG_INFO("handler waiting");
        _stream->close();
    }

    void onConnectionClose() {
        LOG_INFO("connection closed");
        stop();
    }
protected:
    std::shared_ptr<IOStream> _stream;
};


void ConnectionCloseHandler::onGet(StringVector args) {
    Asynchronous()
    _test->onHandlerWaiting();
}

void ConnectionCloseHandler::onConnectionClose() {
    _test->onConnectionClose();
}


class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
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

    void onGet(StringVector args) override {
        std::string path;
        if (!args.empty()) {
            path = std::move(args[0]);
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

    void onGet(StringVector args) override {
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

    void onGet(StringVector args) override {
        setHeader("x-overwrite", "1");
        setHeader("x-overwrite", 2);
        addHeader("x-multi", 3);
        addHeader("x-multi", "4");
    }
};


class TestRedirectHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
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

    void onGet(StringVector args) override {
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

    void onGet(StringVector args) override {
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


class WebTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<OptionalPathHandler>(R"(/optional_path/(.+)?)", "optional_path"),
                url<MultiHeaderHandler>("/multi_header"),
                url<TestRedirectHandler>("/redirect"),
                url<HeaderInjectHandler>("/header_injection"),
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

    void onGet(StringVector args) override {
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

    void onGet(StringVector args) override {
        std::string status = getArgument("status", "");
        if (!status.empty()) {
            sendError(std::stoi(status));
        } else {
            ThrowException(ZeroDivisionError, "integer division or modulo by zero");
        }
    }

    void writeError(int statusCode = 500, std::exception_ptr error= nullptr) override {
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

    void onGet(StringVector args) override {
        ThrowException(ZeroDivisionError, "integer division or modulo by zero");
    }

    void writeError(int statusCode = 500, std::exception_ptr error= nullptr) override {
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


class ClearHeaderTest: public AsyncHTTPTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(StringVector args) override {
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
        Application::LogFunctionType logFunction = [](RequestHandler *) {};
        Application::SettingsType settings = {
                {"logFunction", std::move(logFunction)},
        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms),
                                        std::move(settings));
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

        void onGet(StringVector args) override {
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
        Application::LogFunctionType logFunction = [](RequestHandler *) {};
        Application::SettingsType settings = {
                {"logFunction", std::move(logFunction)},
        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms),
                                        std::move(settings));
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
TINYCORE_TEST_CASE(WebTest, testFlowControl)
TINYCORE_TEST_CASE(WebTest, testEmptyFlush)
TINYCORE_TEST_CASE(ErrorResponseTest, testDefault)
TINYCORE_TEST_CASE(ErrorResponseTest, testWriteError)
TINYCORE_TEST_CASE(ErrorResponseTest, testFailedWriteError)
TINYCORE_TEST_CASE(ClearHeaderTest, testClearHeader)
TINYCORE_TEST_CASE(Header304Test, test304Headers)
