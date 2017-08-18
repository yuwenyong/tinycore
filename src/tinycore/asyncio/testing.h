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
#include "tinycore/configuration/globalinit.h"


struct GlobalFixture {
    GlobalFixture() {
        GlobalInit::initFromEnvironment();
    }

    ~GlobalFixture() {
        GlobalInit::cleanup();
    }
};


class TC_COMMON_API TestCase {
public:
    virtual ~TestCase();

    virtual void setUp();

    virtual void tearDown();

    virtual void handleException(std::exception_ptr error);

    void rethrow() {
        if (_failure) {
            std::exception_ptr failure;
            failure.swap(_failure);
            std::rethrow_exception(failure);
        }
    }
protected:
    std::exception_ptr _failure;
};


class TC_COMMON_API AsyncTestCase: public TestCase {
public:
    typedef std::function<bool ()> ConditionCallback;

    virtual ~AsyncTestCase();

    void setUp() override;

    void tearDown() override;

    void handleException(std::exception_ptr error) override;

    void stop() {
        _stopArgs.clear();
        stopImpl();
    }

    template <typename T>
    void stop(T &&args) {
        _stopArgs = std::forward<T>(args);
        stopImpl();
    }

    template <typename ResultT>
    ResultT wait(boost::optional<float> timeout=5.0f, ConditionCallback condition= nullptr) {
        boost::any result = wait(std::move(timeout), std::move(condition));
        return boost::any_cast<ResultT>(result);
    }

    boost::any wait(boost::optional<float> timeout=5.0f, ConditionCallback condition= nullptr);
protected:
    const IOLoop* ioloop() const {
        return &_ioloop;
    }

    IOLoop* ioloop() {
        return &_ioloop;
    }

    void stopImpl();

    unsigned short getUnusedPort() const {
        static unsigned short nextPort = 10000;
        return ++nextPort;
    }

    IOLoop _ioloop;
    bool _stopped{false};
    bool _running{false};
    boost::any _stopArgs;
    Timeout _timeout;
};


class TC_COMMON_API AsyncHTTPTestCase: public AsyncTestCase {
public:
    virtual ~AsyncHTTPTestCase();
    void setUp() override;
    void tearDown() override;

    template <typename ...Args>
    HTTPResponse fetch(const std::string &path, Args&& ...args) {
        auto request = HTTPRequest::create(getURL(path), std::forward<Args>(args)...);
        return fetch(std::move(request));
    }

    HTTPResponse fetch(std::shared_ptr<HTTPRequest> request) {
        return fetchImpl(std::move(request));
    }

    virtual HTTPResponse fetchImpl(std::shared_ptr<HTTPRequest> request);
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
    unsigned short getHTTPPort() const {
        return _port;
    }

    unsigned short bindUnusedPort() const {
        static unsigned short nextPort = 10000;
        return ++nextPort;
    }

    unsigned short _port;
    std::shared_ptr<HTTPClient> _httpClient;
    std::unique_ptr<Application> _app;
    std::shared_ptr<HTTPServer> _httpServer;
};


class TC_COMMON_API AsyncHTTPSTestCase: public AsyncHTTPTestCase {
public:
    HTTPResponse fetchImpl(std::shared_ptr<HTTPRequest> request) override;
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

#define TINYCORE_TEST_CASE(cls, method, ...)    BOOST_FIXTURE_TEST_CASE(cls##_##method, TestCaseFixture<cls>) { \
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
