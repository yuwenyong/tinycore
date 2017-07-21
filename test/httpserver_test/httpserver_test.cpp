//
// Created by yuwenyong on 17-5-26.
//

#define BOOST_TEST_MODULE httpserver_test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/tinycore.h"

BOOST_TEST_DONT_PRINT_LOG_VALUE(rapidjson::Document)

using namespace rapidjson;

class HandlerBaseTestCase: public AsyncHTTPTestCase {
public:
    template <typename ...Args>
    Document fetchJSON(Args&& ...args) {
        auto response = fetch(std::forward<Args>(args)...);
        response.rethrow();
        const std::string *body = response.getBody();
        Document json;
        if (body) {
            json.Parse(body->c_str());
        }
        return json;
    }
};


class HelloWorldRequestHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override {
        if (args.find("protocol") != args.end()) {
            _expectedProtocol = boost::any_cast<std::string>(args["protocol"]);
        } else {
            _expectedProtocol = "http";
        }
    }

    void onGet(StringVector args) override {
        BOOST_CHECK_EQUAL(_request->getProtocol(), _expectedProtocol);
        finish("Hello world");
    }

    void onPost(StringVector args) override {
        finish(String::format("Got %d bytes in POST", (int)_request->getBody().size()));
    }

protected:
    std::string _expectedProtocol;
};


class SSLTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {

        RequestHandler::ArgsType args = {
                {"protocol", std::string("https") }
        };

        Application::HandlersType handlers = {
                url<HelloWorldRequestHandler>("/", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    std::shared_ptr<SSLOption> getHTTPServerSSLOption() const override {
        auto sslOption = SSLOption::create(true);
        sslOption->setKeyFile("test.key");
        sslOption->setCertFile("test.crt");
        return sslOption;
    }

    void testSSL() {
        do {
            std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
            std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false);
            HTTPResponse response = fetch(std::move(request));
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello world");
        } while (false);
    }

    void testLargePost() {
        do {
            std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
            std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false, ARG_method="POST",
                                                                       ARG_body=std::string(5000, 'A'));
            HTTPResponse response = fetch(std::move(request));
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Got 5000 bytes in POST");
        } while (false);
    }

    void testNonSSLRequest() {
        do {
            HTTPResponse response = fetch("/", ARG_requestTimeout=3600, ARG_connectTimeout=3600);
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }
};


class HTTPConnectionTest: public AsyncHTTPTestCase {
public:
    Application::HandlersType getHandlers() const {
        Application::HandlersType handlers = {
                url<HelloWorldRequestHandler>("/hello"),
        };
        return handlers;
    }

    std::unique_ptr<Application> getApp() const override {
        return make_unique<Application>(getHandlers());
    }

    void test100Continue() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        auto stream = IOStream::create(std::move(socket), &_ioloop);
        stream->connect("localhost", getHTTPPort(), [this]() {
            stop();
        });
        wait();
        StringVector lines = {
                "POST /hello HTTP/1.1",
                "Content-Length: 1024",
                "Expect: 100-continue",
                "Connection: close",
                "\r\n",
        };
        std::string request = boost::join(lines, "\r\n");
        stream->write((const Byte *)request.data(), request.size(), [this]() {
            stop();
        });
        wait();
        stream->readUntil("\r\n\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray received = waitResult<ByteArray>();
        std::string data((const char*)received.data(), received.size());
        BOOST_CHECK(boost::starts_with(data, "HTTP/1.1 100 "));
        std::string send(1024, 'a');
        stream->write((const Byte *)send.data(), send.size());
        stream->readUntil("\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        received = waitResult<ByteArray>();
        std::string firstLine((const char*)received.data(), received.size());
        BOOST_CHECK(boost::starts_with(firstLine, "HTTP/1.1 200"));
        stream->readUntil("\r\n\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        received = waitResult<ByteArray>();
        std::string headerData((const char*)received.data(), received.size());
        auto headers = HTTPHeaders::parse(headerData);
        size_t contentLength = std::stoul(headers->at("Content-Length"));
        stream->readBytes(contentLength, [this](ByteArray data) {
            stop(std::move(data));
        });
        received = waitResult<ByteArray>();
        std::string body((const char*)received.data(), received.size());
        BOOST_CHECK_EQUAL(body, "Got 1024 bytes in POST");
    }
};


class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        auto &arguments = _request->getArguments();
        Document doc;
        Document::AllocatorType &a = doc.GetAllocator();
        doc.SetObject();
        for (auto &arg: arguments) {
            rapidjson::Value values;
            values.SetArray();
            for (auto &val: arg.second) {
                values.PushBack(StringRef(val.c_str(), val.size()), a);
            }
            doc.AddMember(StringRef(arg.first.c_str(), arg.first.size()), values, a);
        }
        write(doc);
    }
};


class HTTPServerTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>("//doubleslash"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testDoubleSlash() {
        do {
            HTTPResponse response = fetch("//doubleslash");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            Document lhs, rhs;
            lhs.Parse(body->c_str());
            BOOST_CHECK_NE(lhs, rhs);
        } while (false);
    }
};


class XHeaderTest: public HandlerBaseTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(StringVector args) override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            std::string remoteIp{_request->getRemoteIp()};
            doc.AddMember("remoteIp", remoteIp, a);
            write(doc);
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    bool getHTTPServerXHeaders() const override {
        return true;
    }

    void testIpHeaders() {
        Document doc;
        HTTPHeaders headers;

        doc = fetchJSON("/");
        BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");

        headers["X-Real-IP"] = "4.4.4.4";
        doc = fetchJSON("/", ARG_headers=headers);
        BOOST_CHECK(doc["remoteIp"] == "4.4.4.4");

        headers["X-Real-IP"] = "2620:0:1cfe:face:b00c::3";
        doc = fetchJSON("/", ARG_headers=headers);
        BOOST_CHECK(doc["remoteIp"] == "2620:0:1cfe:face:b00c::3");

        headers["X-Real-IP"] = "4.4.4.4<script>";
        doc = fetchJSON("/", ARG_headers=headers);
        BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");

        headers["X-Real-IP"] = "www.google.com";
        doc = fetchJSON("/", ARG_headers=headers);
        BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(SSLTest, testSSL)
TINYCORE_TEST_CASE(SSLTest, testLargePost)
TINYCORE_TEST_CASE(SSLTest, testNonSSLRequest)
TINYCORE_TEST_CASE(HTTPConnectionTest, test100Continue)
TINYCORE_TEST_CASE(HTTPServerTest, testDoubleSlash)
TINYCORE_TEST_CASE(XHeaderTest, testIpHeaders)
