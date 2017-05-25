//
// Created by yuwenyong on 17-5-22.
//

#ifndef TINYCORE_TESTING_H
#define TINYCORE_TESTING_H

#include "tinycore/common/common.h"
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
    virtual ~AsyncTestCase();
    void stop();
    void wait();
    void wait(float timeout);
protected:
    IOLoop _ioloop;
    bool _stopped{false};
    bool _running{false};
};


class TC_COMMON_API AsyncHTTPTestCase: public AsyncTestCase {
public:
    typedef HTTPClient::CallbackType HTTPClientCallback;

    virtual ~AsyncHTTPTestCase();

    template <typename ...Args>
    void fetch(const std::string &path, HTTPClientCallback callback, Args&& ...args) {
        onInit();
        _httpClient->fetch(getURL(path), [this, callback](const HTTPResponse &response){
            callback(response);
            stop();
        }, std::forward<Args>(args)...);
        wait();
        onCleanup();
    }

    void fetch(std::shared_ptr<HTTPRequest> request, HTTPClientCallback callback);
    virtual bool getHTTPServerNoKeepAlive() const;
    virtual bool getHTTPServerXHeaders() const;
    virtual std::shared_ptr<SSLOption> getHTTPServerSSLOption() const;
    virtual std::unique_ptr<Application> getApp() const =0;

    std::string getURL(const std::string &path) const {
        return "http://localhost:" + std::to_string(getHTTPPort()) + path;
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

    void onInit();
    void onCleanup();

    bool _inited{false};
    mutable boost::optional<unsigned short> _port;
    std::shared_ptr<HTTPClient> _httpClient;
    std::unique_ptr<Application> _app;
    std::unique_ptr<HTTPServer> _httpServer;
};


template <typename T>
struct TestCaseFixture {
    TestCaseFixture() {}
    ~TestCaseFixture() {}
    T target;
};


#endif //TINYCORE_TESTING_H
