//
// Created by yuwenyong on 17-5-22.
//

#ifndef TINYCORE_TESTING_H
#define TINYCORE_TESTING_H

#include "tinycore/common/common.h"
#include <boost/any.hpp>
#include <boost/optional.hpp>
#include "tinycore/asyncio/httpclient.h"
#include "tinycore/asyncio/ioloop.h"
#include "tinycore/asyncio/web.h"
#include "tinycore/configuration/options.h"


struct GlobalFixture {
    GlobalFixture() {
        sOptions->onEnter();
    }

    ~GlobalFixture() {
        sOptions->onExit();
    }
};


class TC_COMMON_API AsyncTestCase {
public:
    typedef std::function<bool ()> ConditionCallback;

    virtual ~AsyncTestCase();
    virtual void setUp();
    virtual void tearDown();

    void handleException(std::exception_ptr error) {
        _failure = error;
        stop();
    }

    void stop();

    template <typename T>
    void stop(T &&args) {
        _stopArgs = std::move(args);
        stop();
    }

    void wait(boost::optional<float> timeout=5.0f, ConditionCallback condition= nullptr);

    template <typename T>
    T waitResult(boost::optional<float> timeout=5.0f, ConditionCallback condition= nullptr) {
        wait(std::move(timeout), std::move(condition));
        boost::any result(std::move(_stopArgs));
        return boost::any_cast<T>(result);
    }

    void rethrow() {
        if (_failure) {
            std::exception_ptr failure;
            failure.swap(_failure);
            std::rethrow_exception(failure);
        }
    }
protected:
    IOLoop _ioloop;
    bool _stopped{false};
    bool _running{false};
    std::exception_ptr _failure;
    boost::any _stopArgs;
    Timeout _timeout;
};


class TC_COMMON_API AsyncHTTPTestCase: public AsyncTestCase {
public:
    virtual ~AsyncHTTPTestCase();
    virtual void setUp();
    virtual void tearDown();

    template <typename ...Args>
    HTTPResponse fetch(const std::string &path, Args&& ...args) {
        auto request = HTTPRequest::create(getURL(path), std::forward<Args>(args)...);
        return fetch(std::move(request));
    }

    HTTPResponse fetch(std::shared_ptr<HTTPRequest> request);
    virtual std::shared_ptr<HTTPClient> getHTTPClient();
    virtual std::shared_ptr<HTTPServer> getHTTPServer();
    virtual std::unique_ptr<Application> getApp() const =0;
    virtual bool getHTTPServerNoKeepAlive() const;
    virtual bool getHTTPServerXHeaders() const;
    virtual std::shared_ptr<SSLOption> getHTTPServerSSLOption() const;
    virtual std::string getProtocol() const;

    std::string getURL(const std::string &path) const {
        return getProtocol() + "://localhost:" + std::to_string(getHTTPPort()) + path;
    }
protected:
    unsigned short getUnusedPort() const {
        static unsigned short nextPort = 10000;
        return ++nextPort;
    }

    unsigned short getHTTPPort() const {
        if (!_port) {
            _port = getUnusedPort();
        }
        return _port.get();
    }

    mutable boost::optional<unsigned short> _port;
    std::shared_ptr<HTTPClient> _httpClient;
    std::unique_ptr<Application> _app;
    std::shared_ptr<HTTPServer> _httpServer;
};


class TC_COMMON_API AsyncHTTPSTestCase: public AsyncHTTPTestCase {
public:
    using AsyncHTTPTestCase::fetch;

    template <typename ...Args>
    HTTPResponse fetch(const std::string &path, Args&& ...args) {
        auto request = HTTPRequest::create(getURL(path), ARG_validateCert=false, std::forward<Args>(args)...);
        return fetch(std::move(request));
    }

    std::shared_ptr<SSLOption> getHTTPServerSSLOption() const override;
    std::string getProtocol() const override;
};


template <typename T>
struct TestCaseFixture {
    TestCaseFixture() {
        testCase.setUp();
    }
    ~TestCaseFixture() {
        testCase.tearDown();
    }
    T testCase;
};


#define TINYCORE_TEST_INIT()    BOOST_GLOBAL_FIXTURE(GlobalFixture);

#define TINYCORE_TEST_CASE(cls, method, ...)    BOOST_FIXTURE_TEST_CASE(method, TestCaseFixture<cls>) { \
    std::exception_ptr error; \
    try { \
        ExceptionStackContext ctx([&](std::exception_ptr error) { \
            testCase.handleException(std::move(error)); \
        }); \
        testCase.method(##__VA_ARGS__); \
    } catch (...) { \
        error = std::current_exception(); \
    } \
    if (error) { \
        testCase.handleException(error); \
    } \
    testCase.rethrow(); \
} \


#endif //TINYCORE_TESTING_H
