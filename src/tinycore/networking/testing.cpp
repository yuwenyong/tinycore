//
// Created by yuwenyong on 17-5-22.
//

#include "tinycore/networking/testing.h"
#include "tinycore/debugging/trace.h"


AsyncTestCase::~AsyncTestCase() {

}

void AsyncTestCase::stop() {
    if (_running) {
        _ioloop.stop();
        _running = false;
    }
    _stopped = true;
}

void AsyncTestCase::wait() {
    if (!_stopped) {
        if (!_running) {
            _running = true;
            _ioloop.start();
        }
    }
    ASSERT(_stopped);
    _stopped = false;
}

void AsyncTestCase::wait(float timeout) {
    if (!_stopped) {
        _ioloop.addTimeout(timeout, [this](){
            _ioloop.stop();
        });
        if (!_running) {
            _running = true;
            _ioloop.start();
        }
    }
    ASSERT(_stopped);
    _stopped = false;
}


AsyncHTTPTestCase::~AsyncHTTPTestCase() {

}

void AsyncHTTPTestCase::fetch(HTTPRequestPtr request, HTTPClientCallback callback) {
    onInit();
    _httpClient->fetch(std::move(request), [this, callback](const HTTPResponse &response){
        callback(response);
        stop();
    });
    wait();
    onCleanup();
}

bool AsyncHTTPTestCase::getHTTPServerNoKeepAlive() const {
    return false;
}

bool AsyncHTTPTestCase::getHTTPServerXHeaders() const {
    return false;
}

SSLOptionPtr AsyncHTTPTestCase::getHTTPServerSSLOption() const {
    return nullptr;
}

void AsyncHTTPTestCase::onInit() {
    if (!_inited) {
        _httpClient = HTTPClient::create(&_ioloop);
        _app = getApp();
        _httpServer = make_unique<HTTPServer>(HTTPServerCB(*_app), getHTTPServerNoKeepAlive(), &_ioloop,
                                              getHTTPServerXHeaders(), getHTTPServerSSLOption());
        _httpServer->listen(getHTTPPort());
        _inited = true;
    }
}

void AsyncHTTPTestCase::onCleanup() {
    if (_inited) {
        _httpServer->stop();
        _httpClient->close();
        _inited = false;
    }
}