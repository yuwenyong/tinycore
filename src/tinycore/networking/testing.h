//
// Created by yuwenyong on 17-5-22.
//

#ifndef TINYCORE_TESTING_H
#define TINYCORE_TESTING_H

#include "tinycore/common/common.h"
#include <boost/optional.hpp>
#include "tinycore/configuration/options.h"
#include "tinycore/networking/ioloop.h"
#include "tinycore/networking/web.h"
#include "tinycore/networking/httpclient.h"


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

    void fetch(HTTPRequestPtr request, HTTPClientCallback callback);
    virtual bool getHTTPServerNoKeepAlive() const;
    virtual bool getHTTPServerXHeaders() const;
    virtual SSLOptionPtr getHTTPServerSSLOption() const;
    virtual ApplicationPtr getApp() const =0;

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
    HTTPClientPtr _httpClient;
    ApplicationPtr _app;
    HTTPServerPtr _httpServer;
};


template <typename T>
struct TestCaseFixture {
    TestCaseFixture() {}
    ~TestCaseFixture() {}
    T target;
};


#endif //TINYCORE_TESTING_H
