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

void AsyncTestCase::stop() {
    if (_running) {
        _ioloop.stop();
        _running = false;
    }
    _stopped = true;
}

void AsyncTestCase::wait(int timeout, ConditionCallback condition) {
    if (!_stopped) {
        if (timeout > 0) {
            _ioloop.addTimeout(1.0f * timeout, [this, timeout](){
                try {
                    ThrowException(TimeoutError, String::format("Async operation timed out after %d seconds", timeout));
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
    if (_failure) {
        std::rethrow_exception(_failure);
    }
}


AsyncHTTPTestCase::~AsyncHTTPTestCase() {

}

void AsyncHTTPTestCase::setUp() {
    AsyncTestCase::setUp();
    _httpClient = HTTPClient::create(&_ioloop);
    _app = getApp();
    _httpServer = std::make_shared<HTTPServer>(HTTPServerCB(*_app), getHTTPServerNoKeepAlive(), &_ioloop,
                                               getHTTPServerXHeaders(), getHTTPServerSSLOption());
    _httpServer->listen(getHTTPPort(), "127.0.0.1");
}

void AsyncHTTPTestCase::tearDown() {
    _httpServer->stop();
    _httpClient->close();
    AsyncTestCase::tearDown();
}

HTTPResponse AsyncHTTPTestCase::fetch(std::shared_ptr<HTTPRequest> request) {
    _httpClient->fetch(std::move(request), [this](HTTPResponse response){
        stop(std::move(response));
    });
    return waitResult<HTTPResponse>();
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

std::string AsyncHTTPTestCase::getLocalIp() const {
    return "127.0.0.1";
}
