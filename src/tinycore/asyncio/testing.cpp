//
// Created by yuwenyong on 17-5-22.
//

#include "tinycore/asyncio/testing.h"
#include "tinycore/asyncio/stackcontext.h"
#include "tinycore/debugging/trace.h"


AsyncTestCase::~AsyncTestCase() {

}

void AsyncTestCase::setUp() {

}

void AsyncTestCase::tearDown() {

}

void AsyncTestCase::wait(boost::optional<float> timeout, ConditionCallback condition) {
    if (!_stopped) {
        if (timeout) {
            if (!_timeout.expired()) {
                _ioloop.removeTimeout(_timeout);
                _timeout.reset();
            }
            _timeout = _ioloop.addTimeout(*timeout, [this, timeout](){
                try {
                    ThrowException(TimeoutError, String::format("Async operation timed out after %f seconds",
                                                                *timeout));
                } catch (...) {
                    _failure = std::current_exception();
                }
                stop();
            });
        }
        while (true) {
            _running = true;
            do {
                NullContext ctx;
                _ioloop.start();
            } while (false);
            if (_failure || !condition || condition()) {
                break;
            }
        }
    }
    ASSERT(_stopped);
    _stopped = false;
    rethrow();
}

void AsyncTestCase::stopImpl() {
    if (_running) {
        _ioloop.stop();
        _running = false;
    }
    _stopped = true;
}


AsyncHTTPTestCase::~AsyncHTTPTestCase() {

}

void AsyncHTTPTestCase::setUp() {
    AsyncTestCase::setUp();
    _httpClient = getHTTPClient();
    _app = getApp();
    _httpServer = getHTTPServer();
    _httpServer->listen(getHTTPPort(), "127.0.0.1");
}

void AsyncHTTPTestCase::tearDown() {
    _httpServer->stop();
    _httpClient->close();
    AsyncTestCase::tearDown();
}

HTTPResponse AsyncHTTPTestCase::fetchImpl(std::shared_ptr<HTTPRequest> request) {
    _httpClient->fetch(std::move(request), [this](HTTPResponse response){
        stop(std::move(response));
    });
    return waitResult<HTTPResponse>();
}

std::shared_ptr<HTTPClient> AsyncHTTPTestCase::getHTTPClient() {
    return HTTPClient::create(&_ioloop);
}

std::shared_ptr<HTTPServer> AsyncHTTPTestCase::getHTTPServer() {
    return std::make_shared<HTTPServer>(HTTPServerCB(*_app), getHTTPServerNoKeepAlive(), &_ioloop,
                                        getHTTPServerXHeaders(), getHTTPServerSSLOption());
}

bool AsyncHTTPTestCase::getHTTPServerNoKeepAlive() const {
    return false;
}

bool AsyncHTTPTestCase::getHTTPServerXHeaders() const {
    return false;
}

std::shared_ptr<SSLOption> AsyncHTTPTestCase::getHTTPServerSSLOption() const {
    return nullptr;
}

std::string AsyncHTTPTestCase::getProtocol() const {
    return "http";
}


HTTPResponse AsyncHTTPSTestCase::fetchImpl(std::shared_ptr<HTTPRequest> request) {
    request->setValidateCert(false);
    return AsyncHTTPTestCase::fetchImpl(request);
}

std::shared_ptr<SSLOption> AsyncHTTPSTestCase::getHTTPServerSSLOption() const {
    auto sslOption = SSLOption::create(true);
    sslOption->setKeyFile("test.key");
    sslOption->setCertFile("test.crt");
    return sslOption;
}

std::string AsyncHTTPSTestCase::getProtocol() const {
    return "https";
}
