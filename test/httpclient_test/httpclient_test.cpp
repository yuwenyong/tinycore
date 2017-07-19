//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE httpclient_test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/tinycore.h"

BOOST_TEST_DONT_PRINT_LOG_VALUE(std::chrono::milliseconds)

class HelloWorldHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        std::string name = getArgument("name", "world");
        setHeader("Content-Type", "text/plain");
        finish(String::format("Hello %s!", name.c_str()));
    }
};


class PostHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onPost(StringVector args) override {
        std::string arg1 = getArgument("arg1");
        std::string arg2 = getArgument("arg2");
        finish(String::format("Post arg1: %s, arg2: %s", arg1.c_str(), arg2.c_str()));
    }
};


class ChunkHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write("asdf");
        flush();
        write("qwer");
    }
};


class AuthHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        std::string auth = _request->getHTTPHeaders()->at("Authorization");
        finish(auth);
    }
};


class CountDownHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        int count = std::stoi(args[0]);
        if (count > 0) {
            std::string nextCount = std::to_string(count - 1).c_str();
            redirect(reverseURL("countdown", nextCount.c_str()));
        } else {
            write("Zero");
        }
    }
};


class EchoPostHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onPost(StringVector args) override {
        write(_request->getBody());
    }
};



class HTTPClientTestCase: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloWorldHandler>("/hello"),
                url<PostHandler>("/post"),
                url<ChunkHandler>("/chunk"),
                url<AuthHandler>("/auth"),
//                url<HangHandler>("/hang"),
                url<CountDownHandler>(R"(/countdown/([0-9]+))", "countdown"),
                url<EchoPostHandler>("/echopost"),
        };
        std::string defaultHost;
        Application::TransformsType transforms;
        Application::SettingsType settings = {
                {"gzip", true},
        };
        return make_unique<Application>(std::move(handlers), std::move(defaultHost), std::move(transforms),
                                        std::move(settings));
    }

    void testHelloWorld() {
        do {
            HTTPResponse response = fetch("/hello");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-Type"), "text/plain");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello world!");
            auto &requestTime = response.getRequestTime();
            BOOST_CHECK_EQUAL(std::chrono::duration_cast<std::chrono::seconds>(requestTime).count(), 0);
        } while (false);

        do {
            HTTPResponse response = fetch("/hello?name=Ben");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello Ben!");
        } while (false);
    }

    void testStreamingCallback() {
        do {
            StringVector chunks;
            HTTPResponse response = fetch("/hello", ARG_streamingCallback=[&chunks](ByteArray chunk){
                chunks.emplace_back((const char *)chunk.data(), chunk.size());
            });
            BOOST_CHECK_EQUAL(chunks.size(), 1);
            BOOST_CHECK_EQUAL(chunks[0], "Hello world!");
            BOOST_CHECK(!response.getBody() || !response.getBody()->size());
        } while (false);
    }

    void testPost() {
        do {
            HTTPResponse response = fetch("/post", ARG_method="POST", ARG_body="arg1=foo&arg2=bar");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(response.getBody(), static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Post arg1: foo, arg2: bar");
        } while (false);
    }

    void testChunked() {
        do {
            HTTPResponse response = fetch("/chunk");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "asdfqwer");
        } while (false);

        do {
            std::vector<std::string> chunks;
            HTTPResponse response = fetch("/chunk", ARG_streamingCallback=[&chunks](ByteArray chunk){
                chunks.emplace_back((const char *)chunk.data(), chunk.size());
            });
            BOOST_REQUIRE_EQUAL(chunks.size(), 2);
            BOOST_CHECK_EQUAL(chunks[0], "asdf");
            BOOST_CHECK_EQUAL(chunks[1], "qwer");
            BOOST_CHECK(!response.getBody() || !response.getBody()->size());
        } while (false);
    }

    void testChunkedClose() {

        class _Server: public TCPServer {
        public:
            using TCPServer::TCPServer;

            void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override {
                stream->readUntil("\r\n\r\n", [this, stream](ByteArray data) {
                    writeResponse(stream);
                });
            }

            void writeResponse(std::shared_ptr<BaseIOStream> stream) {
                const char *data = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\n1\r\n1\r\n2\r\n0\r\n\r\n";
                stream->write((const Byte *)data, strlen(data), [stream](){
                    stream->close();
                });
            }
        };

        auto port = getUnusedPort();
        auto server = std::make_shared<_Server>(&_ioloop, nullptr);
        server->listen(port);
        _httpClient->fetch(String::format("http://127.0.0.1:%u/", port), [this](HTTPResponse response){
            stop(std::move(response));
        });
        HTTPResponse response = waitResult<HTTPResponse>();
        response.rethrow();
        const std::string *body = response.getBody();
        BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
        BOOST_CHECK_EQUAL(*body, "12");
    }

    void testBasicAuth() {
        do {
            HTTPResponse response = fetch("/auth", ARG_authUserName="Aladdin", ARG_authPassword="open sesame");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
        } while (false);
    }

    void testConnectTimeout() {
        // todo
    }

    void testFollowRedirect() {
        do {
            HTTPResponse response = fetch("/countdown/2", ARG_followRedirects=false);
            BOOST_CHECK_EQUAL(response.getCode(), 302);
            BOOST_CHECK(boost::ends_with(response.getHeaders().at("Location"), "/countdown/1"));
        } while(false);

        do {
            HTTPResponse response = fetch("/countdown/2");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/countdown/0"));
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Zero");
        } while(false);
    }

    void testCredentialsInURL() {
        do {
            std::string url = boost::replace_all_copy(getURL("/auth"), "http://", "http://me:secret@");
            _httpClient->fetch(url, [this](const HTTPResponse &response){
                stop(response);
            });
            HTTPResponse response = waitResult<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Basic " + Base64::b64encode("me:secret"));
        } while (false);

    }

    void testBodyEncoding() {
        // todo
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(HTTPClientTestCase, testHelloWorld)
TINYCORE_TEST_CASE(HTTPClientTestCase, testStreamingCallback)
TINYCORE_TEST_CASE(HTTPClientTestCase, testPost)
TINYCORE_TEST_CASE(HTTPClientTestCase, testChunked)
TINYCORE_TEST_CASE(HTTPClientTestCase, testChunkedClose)
TINYCORE_TEST_CASE(HTTPClientTestCase, testBasicAuth)
TINYCORE_TEST_CASE(HTTPClientTestCase, testFollowRedirect)
TINYCORE_TEST_CASE(HTTPClientTestCase, testCredentialsInURL)


class HangHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        Asynchronous()
//        _request->getConnection()->getStream()->ioloop()->addTimeout(3.0f, [](){
//
//        });
    }
};


class ContentLengthHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        setHeader("Content-Length", getArgument("value"));
        write("ok");
    }
};


class HeadHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onHead(StringVector args) override {
        setHeader("Content-Length", 7);
    }
};


class OptionsHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onOptions(StringVector args) override {
        setHeader("Access-Control-Allow-Origin", "*");
        write("ok");
    }
};


class NoContentHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        auto error = getArgument("error", "");
        if (!error.empty()) {
            setHeader("Content-Length", 7);
        }
        setStatus(204);
    }
};


class SeeOther303PostHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onPost(StringVector args) override {
        BOOST_CHECK_EQUAL(_request->getBody(), "blah");
        setHeader("Location", "/303_get");
        setStatus(303);
    }
};


class SeeOther303GetHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        const auto &body = _request->getBody();
        BOOST_CHECK_EQUAL(body.size(), 0ul);
        write("ok");
    }
};


class HostEchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write(_request->getHTTPHeaders()->at("Host"));
    }
};


class SimpleHTTPClientTestCase: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<ChunkHandler>("/chunk"),
                url<CountDownHandler>(R"(/countdown/([0-9]+))", "countdown"),
                url<HangHandler>("/hang"),
                url<HelloWorldHandler>("/hello"),
                url<ContentLengthHandler>("/content_length"),
                url<HeadHandler>("/head"),
                url<OptionsHandler>("/options"),
                url<NoContentHandler>("/no_content"),
                url<SeeOther303PostHandler>("/303_post"),
                url<SeeOther303GetHandler>("/303_get"),
                url<HostEchoHandler>("/host_echo"),
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
            HTTPHeaders headers;
            headers["Accept-Encoding"] = "gzip";
            HTTPResponse response = fetch("/chunk", ARG_useGzip=false, ARG_headers=std::move(headers));
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-Encoding"), "gzip");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_NE(*body, "asdfqwer");
            BOOST_CHECK_EQUAL(body->size(), 34);
            GzipFile file;
            std::shared_ptr<std::stringstream> stream = std::make_shared<std::stringstream>();
            stream->write(body->data(), body->size());
            file.initWithInputStream(stream);
            BOOST_CHECK_EQUAL(file.readToString(), "asdfqwer");
        } while (false);
    }

    void testMaxRedirects() {
        do {
            HTTPResponse response = fetch("/countdown/5", ARG_maxRedirects=3);
            BOOST_CHECK_EQUAL(response.getCode(), 302);
            BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/countdown/5"));
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/countdown/2"));
            BOOST_CHECK(boost::ends_with(response.getHeaders().at("Location"), "/countdown/1"));
        } while (false);
    }

    void test303Redirect() {
        do {
            HTTPResponse response = fetch("/303_post", ARG_method="POST", ARG_body="blah");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/303_post"));
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/303_get"));
            BOOST_CHECK_EQUAL(response.getRequest()->getMethod(), "POST");
        } while (false);
    }

    void testRequestTimeout() {
        do {
            HTTPResponse response = fetch("/hang", ARG_requestTimeout=0.1f);
            BOOST_REQUIRE_EQUAL(response.getCode(), 599);
            auto requestTime = std::chrono::duration_cast<std::chrono::milliseconds>(response.getRequestTime());
            BOOST_CHECK_LE(std::chrono::milliseconds(99), requestTime);
            BOOST_CHECK_LE(requestTime, std::chrono::milliseconds(110));
            try {
                response.rethrow();
                BOOST_CHECK(false);
            } catch (std::exception &e) {
                std::string error{e.what()};
                BOOST_CHECK(boost::ends_with(error, "HTTP 599: Timeout"));
            }
        } while (false);
    }

    void testIPV6() {
        _httpServer = std::make_shared<HTTPServer>(HTTPServerCB(*_app), getHTTPServerNoKeepAlive(), &_ioloop,
                                                   getHTTPServerXHeaders(), getHTTPServerSSLOption());
        _httpServer->listen(getHTTPPort(), "::1");
        std::string url = boost::replace_all_copy(getURL("/hello"), "localhost", "[::1]");
        _httpClient->fetch(url, [this](HTTPResponse response) {
            stop(response);
        });

        do {
            HTTPResponse response = waitResult<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello world!");
        } while (false);
    }

    void testMultiContentLengthAccepted() {
        do {
            HTTPResponse response = fetch("/content_length?value=2,2");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);

        do {
            HTTPResponse response = fetch("/content_length?value=2,%202,2");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);

        do {
            HTTPResponse response = fetch("/content_length?value=2,4");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);

        do {
            HTTPResponse response = fetch("/content_length?value=2,%202,3");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }

    void testHeadRequest() {
        do {
            HTTPResponse response = fetch("/head", ARG_method="HEAD");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-length"), "7");
            BOOST_CHECK_EQUAL(response.getBody()->size(), 0);
        } while (false);
    }

    void testOptionsRequest() {
        do {
            HTTPResponse response = fetch("/options", ARG_method="OPTIONS");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-length"), "2");
            BOOST_CHECK_EQUAL(response.getHeaders().at("access-control-allow-origin"), "*");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);
    }

    void testNoContent() {
        do {
            HTTPResponse response = fetch("/no_content");
            BOOST_CHECK_EQUAL(response.getCode(), 204);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-length"), "0");
        } while (false);

        do {
            HTTPResponse response = fetch("/no_content?error=1");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }

    void testHostHeader() {
        boost::regex hostRe(R"(^localhost:[0-9]+$)");
        do {
            HTTPResponse response = fetch("/host_echo");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::regex_match(*body, hostRe));
        } while (false);

        do {
            auto url = getURL("/host_echo");
            boost::replace_all(url, "http:://", "http://me:secret@");
            _httpClient->fetch(url, [this](HTTPResponse response) {
                stop(std::move(response));
            });
            HTTPResponse response = waitResult<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::regex_match(*body, hostRe));
        } while (false);
    }
};


TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testGzip)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testMaxRedirects)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, test303Redirect)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testRequestTimeout)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testIPV6)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testMultiContentLengthAccepted)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testHeadRequest)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testOptionsRequest)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testNoContent)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testHostHeader)