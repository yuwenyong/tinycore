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

    void onGet(const StringVector &args) override {
        if (_request->getProtocol() != _expectedProtocol) {
            ThrowException(Exception, "unexpected protocol");
        }
        finish("Hello world");
    }

    void onPost(const StringVector &args) override {
        finish(String::format("Got %d bytes in POST", (int)_request->getBody().size()));
    }

protected:
    std::string _expectedProtocol;
};


class SSLTest: public AsyncHTTPSTestCase {
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

//    std::shared_ptr<SSLOption> getHTTPServerSSLOption() const override {
//        auto sslOption = SSLOption::create(true);
//        sslOption->setKeyFile("test.key");
//        sslOption->setCertFile("test.crt");
//        return sslOption;
//    }

    void testSSL() {
        do {
//            std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
//            std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false);
            HTTPResponse response = fetch("/");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello world");
        } while (false);
    }

    void testLargePost() {
        do {
//            std::string url = boost::replace_first_copy(getURL("/"), "http", "https");
//            std::shared_ptr<HTTPRequest> request = HTTPRequest::create(url, ARG_validateCert=false, ARG_method="POST",
//                                                                       ARG_body=std::string(5000, 'A'));
//            HTTPResponse response = fetch(std::move(request));
            HTTPResponse response = fetch("/", ARG_method="POST", ARG_body=std::string(5000, 'A'));
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Got 5000 bytes in POST");
        } while (false);
    }

    void testNonSSLRequest() {
        do {
            std::string url = boost::replace_first_copy(getURL("/"), "https", "http");
            _httpClient->fetch(url, [this](HTTPResponse response) {
                stop(std::move(response));
            }, ARG_requestTimeout=3600, ARG_connectTimeout=3600);
            HTTPResponse response = wait<HTTPResponse>();
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
        ByteArray received = wait<ByteArray>();
        std::string data((const char*)received.data(), received.size());
        BOOST_CHECK(boost::starts_with(data, "HTTP/1.1 100 "));
        std::string send(1024, 'a');
        stream->write((const Byte *)send.data(), send.size());
        stream->readUntil("\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        received = wait<ByteArray>();
        std::string firstLine((const char*)received.data(), received.size());
        BOOST_CHECK(boost::starts_with(firstLine, "HTTP/1.1 200"));
        stream->readUntil("\r\n\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        received = wait<ByteArray>();
        std::string headerData((const char*)received.data(), received.size());
        auto headers = HTTPHeaders::parse(headerData);
        size_t contentLength = std::stoul(headers->at("Content-Length"));
        stream->readBytes(contentLength, [this](ByteArray data) {
            stop(std::move(data));
        });
        received = wait<ByteArray>();
        std::string body((const char*)received.data(), received.size());
        BOOST_CHECK_EQUAL(body, "Got 1024 bytes in POST");
        stream->close();
    }
};


class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
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

    void onPost(const StringVector &args) override {
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
                url<EchoHandler>("/echo"),
                url<EchoHandler>("//doubleslash"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testEmptyQueryString() {
        do {
            HTTPResponse response = fetch("/echo?foo=&foo=");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            Document lhs, rhs;
            lhs.Parse(body->c_str());
            Document::AllocatorType &a = rhs.GetAllocator();
            rhs.SetObject();
            rapidjson::Value values;
            values.SetArray();
            values.PushBack("", a);
            values.PushBack("", a);
            rhs.AddMember("foo", values, a);
            BOOST_CHECK_EQUAL(lhs, rhs);
        } while (false);
    }

    void testEmptyPostParameters() {
        do {
            HTTPResponse response = fetch("/echo", ARG_method="POST", ARG_body="foo=&bar=");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            Document lhs, rhs;
            lhs.Parse(body->c_str());
            Document::AllocatorType &a = rhs.GetAllocator();
            rhs.SetObject();
            rapidjson::Value fooValues;
            fooValues.SetArray();
            fooValues.PushBack("", a);
            rhs.AddMember("foo", fooValues, a);
            rapidjson::Value barValues;
            barValues.SetArray();
            barValues.PushBack("", a);
            rhs.AddMember("bar", barValues, a);
            BOOST_CHECK_EQUAL(lhs, rhs);
        } while (false);
    }

    void testDoubleSlash() {
        do {
            HTTPResponse response = fetch("//doubleslash");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            Document lhs, rhs;
            lhs.Parse(body->c_str());
            rhs.SetObject();
            BOOST_CHECK_EQUAL(lhs, rhs);
        } while (false);
    }
};


class HTTPServerRawTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>("/echo"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void setUp() override {
        AsyncHTTPTestCase::setUp();
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->connect("localhost", getHTTPPort(), [this]() {
            stop();
        });
        wait();
    }

    void tearDown() override {
        _stream->close();
        AsyncHTTPTestCase::tearDown();
    }

    void testEmptyRequest() {
        _stream->close();
        _ioloop.addTimeout(0.001f, [this]() {
            stop();
        });
        wait();
//        std::cerr << "SUCCESS\n";
    }

    void testMalformedFirstLine() {
        ByteArray request = String::toByteArray("asdf\r\n\r\n");
        _stream->write(request.data(), request.size());
        _ioloop.addTimeout(0.01f, [this]() {
            stop();
        });
        wait();
    }

    void testMalformedHeaders() {
        ByteArray request = String::toByteArray("GET / HTTP/1.0\r\nasdf\r\n\r\n");
        _stream->write(request.data(), request.size());
        _ioloop.addTimeout(0.01f, [this]() {
            stop();
        });
        wait();
    }
protected:
    std::shared_ptr<BaseIOStream> _stream;
};


class XHeaderTest: public HandlerBaseTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            std::string remoteIp{_request->getRemoteIp()};
            std::string remoteProtocol{_request->getProtocol()};
            doc.AddMember("remoteIp", remoteIp, a);
            doc.AddMember("remoteProtocol", remoteProtocol, a);
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
        do {
            Document doc = fetchJSON("/");
            BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Real-IP"] = "4.4.4.4";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "4.4.4.4");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Forwarded-For"] = "127.0.0.1, 4.4.4.4";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "4.4.4.4");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Real-IP"] = "2620:0:1cfe:face:b00c::3";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "2620:0:1cfe:face:b00c::3");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Forwarded-For"] = "::1, 2620:0:1cfe:face:b00c::3";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "2620:0:1cfe:face:b00c::3");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Real-IP"] = "4.4.4.4<script>";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Forwarded-For"] = "4.4.4.4, 5.5.5.5<script>";
            Document doc = fetchJSON("/", ARG_headers=headers);
//            std::cerr << String::fromJSON(doc) << std::endl;
            BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");
        } while (false);

        do {
            HTTPHeaders headers;
            headers["X-Real-IP"] = "www.google.com";
            Document doc = fetchJSON("/", ARG_headers=headers);
            BOOST_CHECK(doc["remoteIp"] == "127.0.0.1");
        } while (false);
    }

    void testSchemeHeaders() {
        Document doc;
        doc = fetchJSON("/");
        BOOST_CHECK(doc["remoteProtocol"] == "http");

        HTTPHeaders httpsScheme;
        httpsScheme["X-Scheme"] = "https";
        doc = fetchJSON("/", ARG_headers=httpsScheme);
        BOOST_CHECK(doc["remoteProtocol"] == "https");

        HTTPHeaders httpsForwarded;
        httpsForwarded["X-Forwarded-Proto"] = "https";
        doc = fetchJSON("/", ARG_headers=httpsForwarded);
        BOOST_CHECK(doc["remoteProtocol"] == "https");

        HTTPHeaders badForwarded;
        badForwarded["X-Forwarded-Proto"] = "unknown";
        doc = fetchJSON("/", ARG_headers=badForwarded);
        BOOST_CHECK(doc["remoteProtocol"] == "http");
    }
};


class SSLXHeaderTest: public AsyncHTTPSTestCase {
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

    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            std::string remoteIp{_request->getRemoteIp()};
            std::string remoteProtocol{_request->getProtocol()};
            doc.AddMember("remoteIp", remoteIp, a);
            doc.AddMember("remoteProtocol", remoteProtocol, a);
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

    void testRequestWithoutXProtocol() {
        Document doc;
        doc = fetchJSON("/");
        BOOST_CHECK(doc["remoteProtocol"] == "https");

        HTTPHeaders httpScheme;
        httpScheme["X-Scheme"] = "http";
        doc = fetchJSON("/", ARG_headers=httpScheme);
        BOOST_CHECK(doc["remoteProtocol"] == "http");

        HTTPHeaders badScheme;
        badScheme["X-Scheme"] = "unknown";
        doc = fetchJSON("/", ARG_headers=badScheme);
        BOOST_CHECK(doc["remoteProtocol"] == "https");
    }
};


class ManualProtocolTest: public HandlerBaseTestCase {
public:
    class Handler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            std::string remoteProtocol{_request->getProtocol()};
            doc.AddMember("protocol", remoteProtocol, a);
            write(doc);
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<Handler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    std::string getHTTPServerProtocol() const override {
        return "https";
    }

    void testManualProtocol() {
        Document doc;
        doc = fetchJSON("/");
        BOOST_CHECK(doc["protocol"] == "https");
    }
};


class KeepAliveTest: public AsyncHTTPTestCase {
public:
    class HelloHandler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            finish("Hello world");
        }
    };

    class LargeHandler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            ByteArray data;
            for (size_t i = 0; i != 512; ++i) {
                ByteArray temp(1024, (Byte)i);
                data.insert(data.end(), temp.begin(), temp.end());
            }
            write(data);
        }
    };

    class FinishOnCloseHandler: public RequestHandler {
    public:
        using RequestHandler::RequestHandler;

        void onGet(const StringVector &args) override {
            Asynchronous();
            flush();
        }

        void onConnectionClose() override {
            finish("Closed");
        }
    };

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloHandler>("/"),
                url<LargeHandler>("/large"),
                url<FinishOnCloseHandler>("/finish_on_close"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void setUp() override {
        AsyncHTTPTestCase::setUp();
        _httpVersion = "HTTP/1.1";
    }

    void tearDown() override {
        _ioloop.addTimeout(0.001f, [this]() {
            stop();
        });
        wait();
        if (_stream) {
            _stream->close();
        }
        AsyncHTTPTestCase::tearDown();
    }

    void connect() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->connect("localhost", getHTTPPort(), [this]() {
            stop();
        });
        wait();
    }

    std::unique_ptr<HTTPHeaders> readHeaders() {
        _stream->readUntil("\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        std::string firstLine = String::toString(wait<ByteArray>());
        BOOST_CHECK(boost::starts_with(firstLine, _httpVersion + " 200"));
        _stream->readUntil("\r\n\r\n", [this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray headerBytes = wait<ByteArray>();
        return HTTPHeaders::parse(String::toString(headerBytes));
    }

    void readResponse() {
        auto headers = readHeaders();
        _stream->readBytes(std::stoul(headers->at("Content-Length")), [this](ByteArray data) {
            stop(std::move(data));
        });
        std::string body = String::toString(wait<ByteArray>());
        BOOST_CHECK_EQUAL("Hello world", body);
    }

    void close() {
        _stream->close();
        _stream.reset();
    }

    void testTwoRequest() {
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.1\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        _stream->write(request.data(), request.size());
        readResponse();
        close();
    }

    void testRequestClose() {
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        _stream->readUntilClose([this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray data = wait<ByteArray>();
        BOOST_CHECK(data.empty());
        close();
    }

    void testHTTP10() {
        _httpVersion = "HTTP/1.0";
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.0\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        _stream->readUntilClose([this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray data = wait<ByteArray>();
        BOOST_CHECK(data.empty());
        close();
    }

    void testHTTP10KeepAlive() {
        _httpVersion = "HTTP/1.0";
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        _stream->write(request.data(), request.size());
        readResponse();
        close();
    }

    void testPipelinedRequests() {
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        readResponse();
        close();
    }

    void testPipelinedCancel() {
        connect();
        ByteArray request = String::toByteArray("GET / HTTP/1.1\r\n\r\nGET / HTTP/1.1\r\n\r\n");
        _stream->write(request.data(), request.size());
        readResponse();
        close();
    }

    void testCancelDuringDownload() {
        connect();
        ByteArray request = String::toByteArray("GET /large HTTP/1.1\r\n\r\n");
        _stream->write(request.data(), request.size());
        readHeaders();
        _stream->readBytes(1024, [this](ByteArray data) {
            stop(std::move(data));
        });
        wait();
        close();
//        std::cerr << "SUC1\n";
    }

    void testFinishWhileClosed() {
        connect();
        ByteArray request = String::toByteArray("GET /finish_on_close HTTP/1.1\r\n\r\n");
        _stream->write(request.data(), request.size());
        readHeaders();
        close();
//        std::cerr << "SUC2\n";
    }
protected:
    std::string _httpVersion;
    std::shared_ptr<BaseIOStream> _stream;
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(SSLTest, testSSL)
TINYCORE_TEST_CASE(SSLTest, testLargePost)
TINYCORE_TEST_CASE(SSLTest, testNonSSLRequest)
TINYCORE_TEST_CASE(HTTPConnectionTest, test100Continue)
TINYCORE_TEST_CASE(HTTPServerTest, testEmptyQueryString)
TINYCORE_TEST_CASE(HTTPServerTest, testEmptyPostParameters)
TINYCORE_TEST_CASE(HTTPServerTest, testDoubleSlash)
TINYCORE_TEST_CASE(HTTPServerRawTest, testEmptyRequest)
TINYCORE_TEST_CASE(HTTPServerRawTest, testMalformedFirstLine)
TINYCORE_TEST_CASE(HTTPServerRawTest, testMalformedHeaders)
TINYCORE_TEST_CASE(XHeaderTest, testIpHeaders)
TINYCORE_TEST_CASE(XHeaderTest, testSchemeHeaders)
TINYCORE_TEST_CASE(SSLXHeaderTest, testRequestWithoutXProtocol)
TINYCORE_TEST_CASE(ManualProtocolTest, testManualProtocol)
TINYCORE_TEST_CASE(KeepAliveTest, testTwoRequest)
TINYCORE_TEST_CASE(KeepAliveTest, testRequestClose)
TINYCORE_TEST_CASE(KeepAliveTest, testHTTP10)
TINYCORE_TEST_CASE(KeepAliveTest, testHTTP10KeepAlive)
TINYCORE_TEST_CASE(KeepAliveTest, testPipelinedRequests)
TINYCORE_TEST_CASE(KeepAliveTest, testPipelinedCancel)
TINYCORE_TEST_CASE(KeepAliveTest, testCancelDuringDownload)
TINYCORE_TEST_CASE(KeepAliveTest, testFinishWhileClosed)