//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE httpclient_test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include "tinycore/tinycore.h"


class HelloWorldHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        std::string name = getArgument("name", "world");
        setHeader("Content-Type", "text/plain");
        std::cerr << String::format("Hello %s!", name.c_str()) << std::endl;
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


class HangHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        Asynchronous()
        _request->getConnection()->fetchStream()->ioloop()->addTimeout(3.0f, [](){

        });

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
                url<HangHandler>("/hang"),
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
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)buffer->data(), buffer->size());
            BOOST_CHECK_EQUAL(body, "Hello world!");
            auto &requestTime = response.getRequestTime();
            BOOST_CHECK_EQUAL(std::chrono::duration_cast<std::chrono::seconds>(requestTime).count(), 0);
        } while (false);
        do {
            HTTPResponse response = fetch("/hello?name=Ben");
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char*)buffer->data(), buffer->size());
            BOOST_CHECK_EQUAL(body, "Hello Ben!");
        } while (false);
    }

//    void testStreamingCallback() {
//        ByteArray chunks;
//        HTTPResponse response = fetch("/hello", streamCallback_=[&chunks](ByteArray chunk){
//            chunks.insert(chunks.end(), chunk.begin(), chunk.end());
//        });
//        do {
//            std::string body((const char*)chunks.data(), chunks.size());
//            BOOST_CHECK_EQUAL(body, "Hello world!");
//        } while (false);
//        do {
//            const ByteArray *buffer = response.getBody();
//            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
//            BOOST_CHECK_EQUAL(buffer->size(), 0);
//        } while (false);
//    }
//
//    void testPost() {
//        std::string body("arg1=foo&arg2=bar");
//        ByteArray requestBody(body.begin(), body.end());
//        HTTPResponse response = fetch("/post", method_="POST", body_=requestBody);
//        BOOST_CHECK_EQUAL(response.getCode(), 200);
//        const ByteArray *responseBody = response.getBody();
//        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//        body.assign((const char*)responseBody->data(), responseBody->size());
//        BOOST_CHECK_EQUAL(body, "Post arg1: foo, arg2: bar");
//    }
//
//    void testChunk() {
//        do {
//            HTTPResponse response = fetch("/chunk");
//            const ByteArray *responseBody = response.getBody();
//            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//            std::string body((const char*)responseBody->data(), responseBody->size());
//            BOOST_CHECK_EQUAL(body, "asdfqwer");
//        } while (false);
//        do {
//            std::vector<ByteArray> chunks;
//            HTTPResponse response = fetch("/chunk", streamCallback_=[&chunks](ByteArray chunk){
//                chunks.emplace_back(std::move(chunk));
//            });
//            std::string chunk;
//            BOOST_REQUIRE_EQUAL(chunks.size(), 2);
//            chunk.assign((const char*)chunks[0].data(), chunks[0].size());
//            BOOST_CHECK_EQUAL(chunk, "asdf");
//            chunk.assign((const char*)chunks[1].data(), chunks[1].size());
//            BOOST_CHECK_EQUAL(chunk, "qwer");
//            const ByteArray *buffer = response.getBody();
//            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
//            BOOST_CHECK_EQUAL(buffer->size(), 0);
//        } while (false);
//    }
//
//    void testBasicAuth() {
//        HTTPResponse response = fetch("/auth", authUserName_="Aladdin", authPassword_="open sesame");
//        const ByteArray *responseBody = response.getBody();
//        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//        std::string body((const char*)responseBody->data(), responseBody->size());
//        BOOST_CHECK_EQUAL(body, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
//    }
//
//    void testGzip() {
//        HTTPHeaders headers;
//        headers["Accept-Encoding"] = "gzip";
//        HTTPResponse response = fetch("/chunk", useGzip_=false, headers_=std::move(headers));
//        BOOST_CHECK_EQUAL(response.getHeaders().at("Content-Encoding"), "gzip");
//        const ByteArray *responseBody = response.getBody();
//        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//        std::string body((const char*)responseBody->data(), responseBody->size());
//        BOOST_CHECK_NE(body, "asdfqwer");
//        BOOST_CHECK_EQUAL(body.size(), 34);
//        GzipFile file;
//        std::shared_ptr<std::stringstream> stream = std::make_shared<std::stringstream>();
//        stream->write(body.data(), body.size());
//        file.initWithInputStream(stream);
//        body = file.readToString();
//        BOOST_CHECK_EQUAL(body, "asdfqwer");
//    }
//
//    void testConnectTimeout() {
//        // todo
//    }
//
//    void testRequestTimeout() {
//        HTTPResponse response = fetch("/hang", ARG_requestTimeout=0.1f);
//        BOOST_REQUIRE_EQUAL(response.getCode(), 599);
//        BOOST_CHECK_EQUAL(*response.getError(), "Timeout");
//    }
//
//    void testFollowRedirect() {
//        do {
//            HTTPResponse response = fetch("/countdown/2", followRedirects_=false);
//            BOOST_CHECK_EQUAL(response.getCode(), 302);
//            BOOST_CHECK(boost::ends_with(response.getHeaders().at("Location"), "/countdown/1"));
//        } while(false);
//        do {
//            HTTPResponse response = fetch("/countdown/2");
//            BOOST_CHECK_EQUAL(response.getCode(), 200);
//            BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/countdown/0"));
//            const ByteArray *responseBody = response.getBody();
//            BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//            std::string body((const char*)responseBody->data(), responseBody->size());
//            BOOST_CHECK_EQUAL(body, "Zero");
//        } while(false);
//    }
//
//    void testMaxRedirects() {
//        HTTPResponse response = fetch("/countdown/5", maxRedirects_=3);
//        BOOST_CHECK_EQUAL(response.getCode(), 302);
//        BOOST_CHECK(boost::ends_with(response.getRequest()->getURL(), "/countdown/5"));
//        BOOST_CHECK(boost::ends_with(response.getEffectiveURL(), "/countdown/2"));
//        BOOST_CHECK(boost::ends_with(response.getHeaders().at("Location"), "/countdown/1"));
//    }
//
//    void testCredentialsInURL() {
//        std::string url = boost::replace_all_copy(getURL("/auth"), "http://", "http://me:secret@");
//        _httpClient->fetch(url, [this](const HTTPResponse &response){
//            stop(response);
//        });
//        HTTPResponse response = waitResult<HTTPResponse>();
//        const ByteArray *responseBody = response.getBody();
//        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//        std::string body((const char*)responseBody->data(), responseBody->size());
//        BOOST_CHECK_EQUAL(body, "Basic " + Base64::b64encode("me:secret"));
//    }
//
//    void testBodyEncoding() {
//        // todo
//    }
//
//    void testIPV6() {
//        std::string url = boost::replace_all_copy(getURL("/hello"), "localhost", "[::1]");
//        _httpClient->fetch(url, [this](const HTTPResponse &response){
//            stop(response);
//        });
//        HTTPResponse response = waitResult<HTTPResponse>();
//        const ByteArray *responseBody = response.getBody();
//        BOOST_REQUIRE_NE(responseBody, static_cast<const ByteArray *>(nullptr));
//        std::string body((const char*)responseBody->data(), responseBody->size());
//        BOOST_CHECK_EQUAL(body, "Hello world!");
//    }
};

TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(HTTPClientTestCase, testHelloWorld)
//
//BOOST_FIXTURE_TEST_CASE(TestHelloWorld, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testHelloWorld();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestStreamingCallback, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testStreamingCallback();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestPost, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testPost();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestChunk, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testChunk();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestBasicAuth, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testBasicAuth();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestGzip, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testGzip();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestConnectTimeout, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testConnectTimeout();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestRequestTimeout, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testRequestTimeout();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestFollowRedirect, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testFollowRedirect();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestMaxRedirects, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testMaxRedirects();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestCredentialsInURL, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testCredentialsInURL();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestBodyEncoding, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testBodyEncoding();
//}
//
//BOOST_FIXTURE_TEST_CASE(TestIPV6, TestCaseFixture<HTTPClientTestCase>) {
//    testCase.testIPV6();
//}