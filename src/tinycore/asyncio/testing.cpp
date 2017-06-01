//
// Created by yuwenyong on 17-5-22.
//

#include "tinycore/asyncio/testing.h"
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

void AsyncTestCase::wait(float timeout, ConditionCallback condition) {
    if (!_stopped) {
        if (timeout > 0.000001f) {
            _ioloop.addTimeout(timeout, [this](){
                stop();
                _failure = true;
            });
        }
        while (true) {
            _running = true;
            _ioloop.start();
            if (_failure || !condition || condition()) {
                break;
            }
        }
    }
    ASSERT(_stopped);
    _stopped = false;
    if (_failure) {
        ThrowException(TimeoutError, String::format("Async operation timed out after %.2f seconds", timeout));
    }
}


AsyncHTTPTestCase::~AsyncHTTPTestCase() {

}

void AsyncHTTPTestCase::setUp() {
    AsyncTestCase::setUp();
    _httpClient = HTTPClient::create(&_ioloop);
    _app = getApp();
    _httpServer = make_unique<HTTPServer>(HTTPServerCB(*_app), getHTTPServerNoKeepAlive(), &_ioloop,
                                          getHTTPServerXHeaders(), getHTTPServerSSLOption());
    _httpServer->listen(getHTTPPort());
}

void AsyncHTTPTestCase::tearDown() {
    _httpServer->stop();
    _httpClient->close();
    AsyncTestCase::tearDown();
}

HTTPResponse AsyncHTTPTestCase::fetch(std::shared_ptr<HTTPRequest> request) {
    _httpClient->fetch(std::move(request), [this](const HTTPResponse &response){
        stop(response);
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
