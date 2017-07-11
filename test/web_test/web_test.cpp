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


class CookieTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<SetCookieHandler>("/set"),
                url<GetCookieHandler>("/get"),
                url<SetCookieDomainHandler>("/set_domain"),
                url<SetCookieSpecialCharHandler>("/special_char"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testSetCookie() {
        HTTPResponse response = fetch("/set");
        StringVector cookies{"str=asdf; Path=/", "unicode=qwer; Path=/", "bytes=zxcv; Path=/"};
        BOOST_CHECK_EQUAL(response.getHeaders().getList("Set-Cookie"), cookies);
    }

    void testGetCookie() {
        do {
            HTTPHeaders headers{{"Cookie", "foo=bar"}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const ByteArray *responseBody = response.getBody();
            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)responseBody->data(), responseBody->size());
            BOOST_CHECK_EQUAL(body, "bar");
        } while (false);

        do {
            HTTPHeaders headers{{"Cookie", "foo=\"bar\""}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const ByteArray *responseBody = response.getBody();
            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)responseBody->data(), responseBody->size());
            BOOST_CHECK_EQUAL(body, "bar");
        } while (false);

        do {
            HTTPHeaders headers{{"Cookie", "/=exception;"}};
            HTTPResponse response = fetch("/get", ARG_headers=headers);
            const ByteArray *responseBody = response.getBody();
            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)responseBody->data(), responseBody->size());
            BOOST_CHECK_EQUAL(body, "default");
        } while (false);
    }

    void testSetCookieDomain() {
        HTTPResponse response = fetch("/set_domain");
        StringVector cookies{"unicode_args=blah; Domain=foo.com; Path=/foo"};
        BOOST_CHECK_EQUAL(response.getHeaders().getList("Set-Cookie"), cookies);
    }

    void testCookieSpecialChar() {
        HTTPResponse response = fetch("/special_char");
        auto headers = response.getHeaders().getList("Set-Cookie");
//        for (auto &cookie: response.getHeaders().getList("Set-Cookie")) {
//            std::cerr << cookie << std::endl;
//        }
        BOOST_CHECK_EQUAL(headers.size(), 3);
        BOOST_CHECK_EQUAL(headers[0], R"(equals="a=b"; Path=/)");
        BOOST_CHECK_EQUAL(headers[1], R"(semicolon="a\073b"; Path=/)");
        BOOST_CHECK_EQUAL(headers[2], R"(quote="a\"b"; Path=/)");
        StringMap data = {{R"(foo=a=b)", "a=b"},
                          {R"(foo="a=b")", "a=b"},
                          {R"(foo="a;b")", "a;b"},
                          {R"(foo="a\073b")", "a;b"},
                          {R"(foo="a\"b")", "a\"b"}};
        for (auto &kv: data) {
            Log::info("trying %s", kv.first.c_str());
            HTTPHeaders requestHeaders{{"Cookie", kv.first}};
            response = fetch("/get", ARG_headers=requestHeaders);
            const ByteArray *responseBody = response.getBody();
            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)responseBody->data(), responseBody->size());
            BOOST_CHECK_EQUAL(body, kv.second);
        }
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
        Log::info("handler waiting");
        _stream->close();
    }

    void onConnectionClose() {
        Log::info("connection closed");
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
        std::string path = args[0];
        auto &arguments = _request->getArguments();
        Document doc;
        Document::AllocatorType &a = doc.GetAllocator();
        doc.SetObject();
        doc.AddMember("path", path, a);
        Value argsObject;
        argsObject.SetObject();
        for (auto &arg: arguments) {
            rapidjson::Value values;
            values.SetArray();
            for (auto &val: arg.second) {
                values.PushBack(StringRef(val.c_str(), val.size()), a);
            }
            argsObject.AddMember(StringRef(arg.first.c_str(), arg.first.size()), values, a);
        }
        doc.AddMember("args", argsObject, a);
        write(doc);
    }
};


class RequestEncodingTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>(R"(/(.*))"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testQuestionMark() {
        using namespace rapidjson;
        do {
            HTTPResponse response = fetch("/%3F");
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)buffer->data(), buffer->size());
            Document json;
            json.Parse(body.c_str());

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "?", a);
            Value argsObj;
            argsObj.SetObject();
            doc.AddMember("args", argsObj, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);
        do {
            HTTPResponse response = fetch("/%3F?%3F=%3F");
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)buffer->data(), buffer->size());
            Document json;
            json.Parse(body.c_str());

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "?", a);
            Value argsObj;
            argsObj.SetObject();
            Value values;
            values.SetArray();
            values.PushBack("?", a);
            argsObj.AddMember("?", values, a);
            doc.AddMember("args", argsObj, a);
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
        doc.AddMember("path", path, a);
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


class WebTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<OptionalPathHandler>(R"(/optional_path/(.+)?)"),
                url<FlowControlHandler>("/flow_control"),
                url<MultiHeaderHandler>("/multi_header"),
                url<TestRedirectHandler>("/redirect"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    template <typename ...Args>
    Document fetchJSON(const std::string &path, Args&& ...args) {
        HTTPResponse response = fetch(path, std::forward<Args>(args)...);
        response.rethrow();
        const ByteArray *bytes = response.getBody();
        BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
        Document json;
        json.Parse((const char*)bytes->data(), bytes->size());
        return json;
    }

    void testOptionalPath() {
        do {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "foo", a);
            BOOST_CHECK_EQUAL(fetchJSON("/optional_path/foo"), doc);
        } while (false);
        do {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "", a);
            BOOST_CHECK_EQUAL(fetchJSON("/optional_path/"), doc);
        } while (false);
    }

    void testFlowControl() {
        HTTPResponse response = fetch("/flow_control");
        const ByteArray *bytes = response.getBody();
        BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
        std::string body((const char *)bytes->data(), bytes->size());
        BOOST_CHECK_EQUAL(body, "123");
    }

    void testMultiHeader() {
        HTTPResponse response = fetch("/multi_header");
        auto &headers = response.getHeaders();
        BOOST_CHECK_EQUAL(headers.at("x-overwrite"), "2");
        StringVector rhs{"3", "4"};
        BOOST_CHECK_EQUAL(headers.getList("x-multi"), rhs);
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
            const ByteArray *bytes = response.getBody();
            BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)bytes->data(), bytes->size());
            BOOST_CHECK(boost::contains(body, "500: Internal Server Error"));
        } while (false);

        do {
            HTTPResponse response = fetch("/default?status=503");
            BOOST_CHECK_EQUAL(response.getCode(), 503);
            const ByteArray *bytes = response.getBody();
            BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)bytes->data(), bytes->size());
            BOOST_CHECK(boost::contains(body, "503: Service Unavailable"));
        } while (false);
    }

    void testWriteError() {
        do {
            HTTPResponse response = fetch("/write_error");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
            const ByteArray *bytes = response.getBody();
            BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)bytes->data(), bytes->size());
            BOOST_CHECK(boost::contains(body, "ZeroDivisionError"));
        } while (false);

        do {
            HTTPResponse response = fetch("/write_error?status=503");
            BOOST_CHECK_EQUAL(response.getCode(), 503);
            const ByteArray *bytes = response.getBody();
            BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)bytes->data(), bytes->size());
            BOOST_CHECK_EQUAL(body, "Status: 503");
        } while (false);
    }

    void testFailedWriteError() {
        do {
            HTTPResponse response = fetch("/failed_write_error");
            BOOST_CHECK_EQUAL(response.getCode(), 500);
            const ByteArray *bytes = response.getBody();
            BOOST_REQUIRE_NE(bytes, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)bytes->data(), bytes->size());
            BOOST_CHECK_EQUAL(body, "");
        } while (false);
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(CookieTest, testSetCookie)
TINYCORE_TEST_CASE(CookieTest, testGetCookie)
TINYCORE_TEST_CASE(CookieTest, testSetCookieDomain)
TINYCORE_TEST_CASE(CookieTest, testCookieSpecialChar)
TINYCORE_TEST_CASE(ConnectionCloseTest, testConnectionClose)
TINYCORE_TEST_CASE(RequestEncodingTest, testQuestionMark)
TINYCORE_TEST_CASE(WebTest, testOptionalPath)
TINYCORE_TEST_CASE(WebTest, testFlowControl)
TINYCORE_TEST_CASE(WebTest, testMultiHeader)
TINYCORE_TEST_CASE(WebTest, testRedirect)
TINYCORE_TEST_CASE(ErrorResponseTest, testDefault)
TINYCORE_TEST_CASE(ErrorResponseTest, testWriteError)
TINYCORE_TEST_CASE(ErrorResponseTest, testFailedWriteError)
