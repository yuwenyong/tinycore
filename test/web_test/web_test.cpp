//
// Created by yuwenyong on 17-5-22.
//

#define BOOST_TEST_MODULE web_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_TEST_DONT_PRINT_LOG_VALUE(StringVector)


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
        write(getCookie("foo"));
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
        HTTPHeaders headers{{"Cookie", "foo=bar"}};
        HTTPResponse response = fetch("/get", ARG_headers=headers);
        const ByteArray *responseBody = response.getBody();
        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
        std::string body((const char*)responseBody->data(), responseBody->size());
        BOOST_CHECK_EQUAL(body, "bar");
    }

    void testSetCookieDomain() {
        HTTPResponse response = fetch("/set_domain");
        StringVector cookies{"unicode_args=blah; Domain=foo.com; Path=/foo"};
        BOOST_CHECK_EQUAL(response.getHeaders().getList("Set-Cookie"), cookies);
    }

    void testCookieSpecialChar() {
//        HTTPResponse response = fetch("/special_char");
//        auto headers = response.getHeaders().getList("Set-Cookie");
//        for (auto &cookie: response.getHeaders().getList("Set-Cookie")) {
//            std::cerr << cookie << std::endl;
//        }
//        BOOST_CHECK_EQUAL(headers.size(), 3);
//        BOOST_CHECK_EQUAL(headers[0], R"(equals="a=b"; Path=/)");
//        BOOST_CHECK_EQUAL(headers[1], R"(semicolon="a\073b"; Path=/)");
//        BOOST_CHECK_EQUAL(headers[2], R"(quote="a\"b"; Path=/)");
        StringMap data = {{R"(foo=a=b)", "a=b"},
                          {R"(foo="a=b")", "a=b"},
                          {R"(foo="a;b")", "a;b"},
                          {R"(foo="a\073b")", "a;b"},
                          {R"(foo="a\"b")", "a\"b"}};
        for (auto &kv: data) {
            Log::info("trying %s", kv.first.c_str());
            HTTPHeaders requestHeaders{{"Cookie", kv.first}};
            HTTPResponse response = fetch("/get", ARG_headers=requestHeaders);
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
        _stream->connect("localhost", getHTTPPort(), std::bind(&ConnectionCloseTest::onConnect, this));
        wait();
    }

    void onConnect() {
        const char *data = "GET / HTTP/1.0\r\n\r\n";
        _stream->write((const Byte *)data, strlen(data));
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

BOOST_TEST_DONT_PRINT_LOG_VALUE(rapidjson::Document)

class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        using namespace rapidjson;
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

TINYCORE_TEST_INIT()
//TINYCORE_TEST_CASE(CookieTest, testSetCookie)
//TINYCORE_TEST_CASE(CookieTest, testGetCookie)
//TINYCORE_TEST_CASE(CookieTest, testSetCookieDomain)
TINYCORE_TEST_CASE(CookieTest, testCookieSpecialChar)
//TINYCORE_TEST_CASE(ConnectionCloseTest, testConnectionClose)
//TINYCORE_TEST_CASE(RequestEncodingTest, testQuestionMark)


