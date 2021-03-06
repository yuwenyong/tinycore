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

    void onGet(const StringVector &args) override {
        std::string name = getArgument("name", "world");
        setHeader("Content-Type", "text/plain");
        finish(String::format("Hello %s!", name.c_str()));
    }
};


class PostHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onPost(const StringVector &args) override {
        std::string arg1 = getArgument("arg1");
        std::string arg2 = getArgument("arg2");
        finish(String::format("Post arg1: %s, arg2: %s", arg1.c_str(), arg2.c_str()));
    }
};


class ChunkHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        write("asdf");
        flush();
        write("qwer");
    }
};


class AuthHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        std::string auth = _request->getHTTPHeaders()->at("Authorization");
        finish(auth);
    }
};


class CountDownHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
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

    void onPost(const StringVector &args) override {
        write(_request->getBody());
    }
};


class AllMethodHandler: public RequestHandler {
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

    void onPatch(const StringVector &args) override {
        onMethod();
    }

    void onMethod() {
        write(_request->getMethod());
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
                url<AllMethodHandler>("/all_methods"),
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

        auto server = std::make_shared<_Server>(&_ioloop, nullptr);
        server->listen(0);
        auto port = server->getLocalPort();
        _httpClient->fetch(String::format("http://127.0.0.1:%u/", port), [this](HTTPResponse response){
            stop(std::move(response));
        });
        HTTPResponse response = wait<HTTPResponse>();
        response.rethrow();
        const std::string *body = response.getBody();
        BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
        BOOST_CHECK_EQUAL(*body, "12");
    }

    void testStreamingStackContext() {
        std::vector<std::string> chunks;
        std::vector<std::exception_ptr> errors;
        ExceptionStackContext ctx([&errors](std::exception_ptr error) {
           errors.emplace_back(std::move(error));
        });
        fetch("/chunk", ARG_streamingCallback = [&chunks](ByteArray chunk) {
            chunks.emplace_back((const char *)chunk.data(), chunk.size());
            if (String::toString(chunk) == "qwer") {
                ThrowException(ZeroDivisionError, "");
            }
        });
        BOOST_REQUIRE_EQUAL(chunks.size(), 2);
        BOOST_CHECK_EQUAL(chunks[0], "asdf");
        BOOST_CHECK_EQUAL(chunks[1], "qwer");
        BOOST_REQUIRE_EQUAL(errors.size(), 1);
        try {
            std::rethrow_exception(errors[0]);
        } catch (ZeroDivisionError &e) {
//            std::cerr << "SUCCESS";
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testBasicAuth() {
        do {
            HTTPResponse response = fetch("/auth", ARG_authUserName="Aladdin", ARG_authPassword="open sesame");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
        } while (false);
    }

    void testBasicAuthExplicitMode() {
        do {
            HTTPResponse response = fetch("/auth", ARG_authUserName="Aladdin", ARG_authPassword="open sesame",
                                          ARG_authMode="basic");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
        } while (false);
    }

    void testUnsupportAuthMode() {
        do {
            HTTPResponse response = fetch("/auth", ARG_authUserName="Aladdin", ARG_authPassword="open sesame",
                                          ARG_authMode="asdf");
            BOOST_REQUIRE(response.getError());
            try {
                std::rethrow_exception(response.getError());
            } catch (ValueError &e) {
                LOG_INFO(e.what());
            } catch (HTTPError &e) {
                LOG_INFO(e.what());
            } catch (...) {
                BOOST_CHECK(false);
            }
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
            HTTPResponse response = wait<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Basic " + Base64::b64encode("me:secret"));
        } while (false);

    }

    void testBodyEncoding() {
        // todo
    }

    void testHeaderCallback() {
        StringVector firstLine;
        StringMap headers;
        StringVector chunks;
        fetch("/chunk", ARG_headerCallback = [&firstLine, &headers](std::string headerLine) {
            if (boost::starts_with(headerLine, "HTTP/")) {
                firstLine.emplace_back(std::move(headerLine));
            } else if (headerLine != "\r\n") {
                std::string key, value;
                auto pos = headerLine.find(':');
                if (pos != std::string::npos) {
                    key = headerLine.substr(0, pos);
                    value = headerLine.substr(pos + 1);
                } else {
                    key = std::move(headerLine);
                }
                headers[key] = boost::trim_copy(value);
            }
        }, ARG_streamingCallback = [&headers, &chunks](ByteArray chunk) {
            BOOST_CHECK_EQUAL(headers.at("Content-Type"), "text/html; charset=UTF-8");
            chunks.emplace_back((const char *)chunk.data(), chunk.size());
        });
        BOOST_CHECK_EQUAL(firstLine.size(), 1);
        const boost::regex pattern("HTTP/1.[01] 200 OK\r\n");
        BOOST_CHECK(boost::regex_match(firstLine[0], pattern));
        BOOST_REQUIRE_EQUAL(chunks.size(), 2);
        BOOST_CHECK_EQUAL(chunks[0], "asdf");
        BOOST_CHECK_EQUAL(chunks[1], "qwer");
    }

    void testHeaderCallbackStackContext() {
        std::vector<std::exception_ptr> errors;
        ExceptionStackContext ctx([&errors](std::exception_ptr error) {
            errors.emplace_back(std::move(error));
        });
        fetch("/chunk", ARG_headerCallback = [](std::string headerLine) {
            if (boost::starts_with(headerLine, "Content-Type:")) {
                ThrowException(ZeroDivisionError, "");
            }
        });
        BOOST_REQUIRE_EQUAL(errors.size(), 1);
        try {
            std::rethrow_exception(errors[0]);
        } catch (ZeroDivisionError &e) {
//            std::cerr << "SUCCESS";
        } catch (...) {
            BOOST_CHECK(false);
        }
    }

    void testReuseRequestFromResponse() {
        std::string url = getURL("/hello");
        _httpClient->fetch(url, [this](HTTPResponse response) {
            stop(std::move(response));
        });
        HTTPResponse response = wait<HTTPResponse>();
        BOOST_CHECK_EQUAL(response.getRequest()->getURL(), url);
        _httpClient->fetch(response.getRequest(), [this](HTTPResponse response) {
            stop(std::move(response));
        });
        HTTPResponse response2 = wait<HTTPResponse>();
        const std::string *body = response.getBody();
        BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
        BOOST_CHECK_EQUAL(*body, "Hello world!");
    }

    void testAllMethods() {
        for (std::string method: {"GET", "DELETE", "OPTIONS"}) {
            HTTPResponse response = fetch("/all_methods", ARG_method=method);
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, method);
        }

        for (std::string method: {"POST", "PUT", "PATCH"}) {
            HTTPResponse response = fetch("/all_methods", ARG_method=method, ARG_body="");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, method);
        }

        do {
            HTTPResponse response = fetch("/all_methods", ARG_method="HEAD");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "");
        } while (false);
    }

    void testBody() {
        do {
            HTTPResponse response = fetch("/hello", ARG_body="data");
            BOOST_REQUIRE(response.getError());
            try {
                std::rethrow_exception(response.getError());
            } catch (AssertionError &e) {
                BOOST_CHECK(boost::contains(e.what(), "must be empty"));
            } catch (...) {
                BOOST_CHECK(false);
            }
        } while (false);

        do {
            HTTPResponse response = fetch("/hello", ARG_method="POST");
            BOOST_REQUIRE(response.getError());
            try {
                std::rethrow_exception(response.getError());
            } catch (AssertionError &e) {
                BOOST_CHECK(boost::contains(e.what(), "must not be empty"));
            } catch (...) {
                BOOST_CHECK(false);
            }
        } while (false);
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(HTTPClientTestCase, testHelloWorld)
TINYCORE_TEST_CASE(HTTPClientTestCase, testStreamingCallback)
TINYCORE_TEST_CASE(HTTPClientTestCase, testPost)
TINYCORE_TEST_CASE(HTTPClientTestCase, testChunked)
TINYCORE_TEST_CASE(HTTPClientTestCase, testChunkedClose)
TINYCORE_TEST_CASE(HTTPClientTestCase, testStreamingStackContext)
TINYCORE_TEST_CASE(HTTPClientTestCase, testBasicAuth)
TINYCORE_TEST_CASE(HTTPClientTestCase, testBasicAuthExplicitMode)
TINYCORE_TEST_CASE(HTTPClientTestCase, testUnsupportAuthMode)
TINYCORE_TEST_CASE(HTTPClientTestCase, testFollowRedirect)
TINYCORE_TEST_CASE(HTTPClientTestCase, testCredentialsInURL)
TINYCORE_TEST_CASE(HTTPClientTestCase, testHeaderCallback)
TINYCORE_TEST_CASE(HTTPClientTestCase, testHeaderCallbackStackContext)
TINYCORE_TEST_CASE(HTTPClientTestCase, testReuseRequestFromResponse)
TINYCORE_TEST_CASE(HTTPClientTestCase, testAllMethods)
TINYCORE_TEST_CASE(HTTPClientTestCase, testBody)


class HTTPResponseTestCase: public TestCase {
public:
    void testStr() {
        auto request = HTTPRequest::create("http://example.com");
        HTTPResponse response(request, 200, {}, {}, {}, {}, {});
        auto s = response.toString();
//        std::cerr << s << std::endl;
        BOOST_CHECK(boost::starts_with(s, "HTTPResponse("));
        BOOST_CHECK(boost::contains(s, "code=200"));
    }
};


TINYCORE_TEST_CASE(HTTPResponseTestCase, testStr)


class HangHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        Asynchronous()
//        _request->getConnection()->getStream()->ioloop()->addTimeout(3.0f, [](){
//
//        });
    }
};


class ContentLengthHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        setHeader("Content-Length", getArgument("value"));
        write("ok");
    }
};


class HeadHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onHead(const StringVector &args) override {
        setHeader("Content-Length", 7);
    }
};


class OptionsHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onOptions(const StringVector &args) override {
        setHeader("Access-Control-Allow-Origin", "*");
        write("ok");
    }
};


class NoContentHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        auto error = getArgument("error", "");
        if (!error.empty()) {
            setHeader("Content-Length", 7);
        }
        setStatus(204);
    }
};


class SeeOtherPostHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onPost(const StringVector &args) override {
        int redirectCode = std::stoi(_request->getBody());
        ASSERT(redirectCode == 302 || redirectCode == 303, "unexpected body %s", _request->getBody().c_str());
        setHeader("Location", "/see_other_get");
        setStatus(redirectCode);
    }
};


class SeeOtherGetHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        if (!_request->getBody().empty()) {
            ThrowException(Exception, String::format("unexpected body %s", _request->getBody().c_str()));
        }
        write("ok");
    }
};


class HostEchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(const StringVector &args) override {
        write(_request->getHTTPHeaders()->at("Host"));
    }
};


template <typename BaseT>
class SimpleHTTPClientTestMixin: public BaseT {
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
                url<SeeOtherPostHandler>("/see_other_post"),
                url<SeeOtherGetHandler>("/see_other_get"),
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
            HTTPResponse response = this->fetch("/chunk", ARG_useGzip=false, ARG_headers=std::move(headers));
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
            HTTPResponse response = this->fetch("/countdown/5", ARG_maxRedirects=3);
            BOOST_CHECK_EQUAL(response.getCode(), 302);
            BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/countdown/5"));
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/countdown/2"));
            BOOST_CHECK(boost::ends_with(response.getHeaders().at("Location"), "/countdown/1"));
        } while (false);
    }

    void testSeeOtherRedirect() {
        do {
            HTTPResponse response = this->fetch("/see_other_post", ARG_method="POST", ARG_body="302");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/see_other_post"));
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/see_other_get"));
            BOOST_CHECK_EQUAL(response.getRequest()->getMethod(), "POST");
        } while (false);

        do {
            HTTPResponse response = this->fetch("/see_other_post", ARG_method="POST", ARG_body="303");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/see_other_post"));
            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/see_other_get"));
            BOOST_CHECK_EQUAL(response.getRequest()->getMethod(), "POST");
        } while (false);
    }

    void testRequestTimeout() {
        do {
            HTTPResponse response = this->fetch("/hang", ARG_requestTimeout=0.1f);
            BOOST_REQUIRE_EQUAL(response.getCode(), 599);
            auto requestTime = std::chrono::duration_cast<std::chrono::milliseconds>(response.getRequestTime());
            BOOST_CHECK_LE(std::chrono::milliseconds(99), requestTime);
            BOOST_CHECK_LE(requestTime, std::chrono::milliseconds(150));
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
        this->_httpServer = std::make_shared<HTTPServer>(HTTPServerCB(*this->_app), this->getHTTPServerNoKeepAlive(),
                                                         &this->_ioloop, this->getHTTPServerXHeaders(), "",
                                                         this->getHTTPServerSSLOption());
        this->_httpServer->listen(this->getHTTPPort(), "::1");
        std::string url = boost::replace_all_copy(this->getURL("/hello"), "localhost", "[::1]");
        this->_httpClient->fetch(url, [this](HTTPResponse response) {
            this->stop(response);
        }, ARG_validateCert=false);

        do {
            HTTPResponse response = this->template wait<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "Hello world!");
        } while (false);
    }

    void testMultiContentLengthAccepted() {
        do {
            HTTPResponse response = this->fetch("/content_length?value=2,2");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);

        do {
            HTTPResponse response = this->fetch("/content_length?value=2,%202,2");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "ok");
        } while (false);

        do {
            HTTPResponse response = this->fetch("/content_length?value=2,4");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);

        do {
            HTTPResponse response = this->fetch("/content_length?value=2,%202,3");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }

    void testHeadRequest() {
        do {
            HTTPResponse response = this->fetch("/head", ARG_method="HEAD");
            BOOST_CHECK_EQUAL(response.getCode(), 200);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-length"), "7");
            BOOST_CHECK_EQUAL(response.getBody()->size(), 0);
        } while (false);
    }

    void testOptionsRequest() {
        do {
            HTTPResponse response = this->fetch("/options", ARG_method="OPTIONS");
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
            HTTPResponse response = this->fetch("/no_content");
            BOOST_CHECK_EQUAL(response.getCode(), 204);
            BOOST_CHECK_EQUAL(response.getHeaders().at("Content-length"), "0");
        } while (false);

        do {
            HTTPResponse response = this->fetch("/no_content?error=1");
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }

    void testHostHeader() {
        boost::regex hostRe(R"(^localhost:[0-9]+$)");
        do {
            HTTPResponse response = this->fetch("/host_echo");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::regex_match(*body, hostRe));
        } while (false);

        do {
            auto url = this->getURL("/host_echo");
            boost::replace_all(url, "http:://", "http://me:secret@");
            this->_httpClient->fetch(url, [this](HTTPResponse response) {
                this->stop(std::move(response));
            }, ARG_validateCert=false);
            HTTPResponse response = this->template wait<HTTPResponse>();
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK(boost::regex_match(*body, hostRe));
        } while (false);
    }

    void testConnectionRefused() {
        do {
            auto port = this->getUnusedPort();
            this->_httpClient->fetch(String::format("http://localhost:%u/", port), [this] (HTTPResponse response) {
                this->stop(std::move(response));
            }, ARG_validateCert=false);
            HTTPResponse response = this->template wait<HTTPResponse>();
            BOOST_CHECK_EQUAL(response.getCode(), 599);
        } while (false);
    }
};


using SimpleHTTPClientTestCase = SimpleHTTPClientTestMixin<AsyncHTTPTestCase>;
using SimpleHTTPSClientTestCase = SimpleHTTPClientTestMixin<AsyncHTTPSTestCase>;


TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testGzip)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testMaxRedirects)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testSeeOtherRedirect)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testRequestTimeout)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testIPV6)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testMultiContentLengthAccepted)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testHeadRequest)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testOptionsRequest)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testNoContent)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testHostHeader)
TINYCORE_TEST_CASE(SimpleHTTPClientTestCase, testConnectionRefused)

TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testGzip)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testMaxRedirects)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testSeeOtherRedirect)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testRequestTimeout)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testIPV6)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testMultiContentLengthAccepted)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testHeadRequest)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testOptionsRequest)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testNoContent)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testHostHeader)
TINYCORE_TEST_CASE(SimpleHTTPSClientTestCase, testConnectionRefused)


class HTTP100ContinueTestCase: public AsyncHTTPTestCase {
public:
    void respond100(std::shared_ptr<HTTPServerRequest> request) {
        const char *data = "HTTP/1.1 100 CONTINUE\r\n\r\n";
        request->getConnection()->getStream()->write((const Byte *)data, strlen(data),
                                                     std::bind(&HTTP100ContinueTestCase::respond200, this, request));
    }

    void respond200(std::shared_ptr<HTTPServerRequest> request) {
        const char *data = "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nA";
        request->getConnection()->getStream()->write((const Byte *)data, strlen(data), [request]() {
            request->getConnection()->getStream()->close();
        });
    }

    std::unique_ptr<Application> getApp() const override {
        return nullptr;
    }

    void test100Continue() {
        _httpServer->stop();
        _httpServer = std::make_shared<HTTPServer>(std::bind(&HTTP100ContinueTestCase::respond100, this,
                                                             std::placeholders::_1), getHTTPServerNoKeepAlive(),
                                                   &_ioloop, getHTTPServerXHeaders(), "", getHTTPServerSSLOption());
        _httpServer->listen(getHTTPPort(), "127.0.0.1");

        do {
            HTTPResponse response = fetch("/");
            const std::string *body = response.getBody();
            BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
            BOOST_CHECK_EQUAL(*body, "A");
//            std::cerr << "FINISHED\n";
        } while (false);
    }
};

TINYCORE_TEST_CASE(HTTP100ContinueTestCase, test100Continue)


class HostnameMappingTestCase: public AsyncHTTPTestCase {
public:
    std::shared_ptr<HTTPClient> getHTTPClient() override {
        StringMap hostnameMapping = {{"www.example.com", "127.0.0.1"}};
        return HTTPClient::create(&_ioloop, hostnameMapping);
    }

    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloWorldHandler>("/hello"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testHostnameMapping() {
        _httpClient->fetch(String::format("http://www.example.com:%d/hello", getHTTPPort()),
                           [this](HTTPResponse response) {
                               stop(std::move(response));
                           });
        HTTPResponse response = wait<HTTPResponse>();
        response.rethrow();
        const std::string *body = response.getBody();
        BOOST_REQUIRE_NE(body, static_cast<const std::string *>(nullptr));
        BOOST_CHECK_EQUAL(*body, "Hello world!");
//        std::cerr << "FINISHED\n";
    }
};

TINYCORE_TEST_CASE(HostnameMappingTestCase, testHostnameMapping)