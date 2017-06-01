//
// Created by yuwenyong on 17-5-22.
//

#define BOOST_TEST_MODULE web_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


class ConnectionCloseTest;

class ConnectionCloseHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void initialize(ArgsType &args) override {
        _test = boost::any_cast<ConnectionCloseTest *>(args["test"]);
    }

    void onGet(StringVector args) override;
    void onConnectionClose() override;
protected:
    ConnectionCloseTest *_test;
};


class ConnectionCloseTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        RequestHandler::ArgsType args = {
                {"test", const_cast<ConnectionCloseTest *>(this) }
        };

        Application::HandlersType handlers = {
                url<ConnectionCloseHandler>("/", args),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testConnectionClose() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        _stream = IOStream::create(std::move(socket), &_ioloop);
        _stream->connect("localhost", getHTTPPort(), std::bind(&ConnectionCloseTest::onConnect, this));
        wait();
    }

    void onConnect() {
        const char *data = "GET / HTTP/1.0\r\n\r\n";
        _stream->write((const Byte *)data, strlen(data));
    }

    void onHandlerWaiting() {
        Log::info("handler waiting");
        _stream->close();
    }

    void onConnectionClose() {
        Log::info("connection closed");
        stop();
    }
protected:
    std::shared_ptr<IOStream> _stream;
};


void ConnectionCloseHandler::onGet(StringVector args) {
    Asynchronous()
    _test->onHandlerWaiting();
}

void ConnectionCloseHandler::onConnectionClose() {
    _test->onConnectionClose();
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(rapidjson::Document)

class EchoHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        using namespace rapidjson;
        std::string path = args[0];
        auto &arguments = _request->getArguments();
        Document doc;
        Document::AllocatorType &a = doc.GetAllocator();
        doc.SetObject();
        doc.AddMember("path", path, a);
        Value argsObject;
        argsObject.SetObject();
        for (auto &arg: arguments) {
            rapidjson::Value values;
            values.SetArray();
            for (auto &val: arg.second) {
                values.PushBack(StringRef(val.c_str(), val.size()), a);
            }
            argsObject.AddMember(StringRef(arg.first.c_str(), arg.first.size()), values, a);
        }
        doc.AddMember("args", argsObject, a);
        write(doc);
    }
};


class RequestEncodingTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<EchoHandler>(R"(/(.*))"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testQuestionMark() {
        using namespace rapidjson;
        do {
            HTTPResponse response = fetch("/%3F");
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)buffer->data(), buffer->size());
            Document json;
            json.Parse(body.c_str());

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "?", a);
            Value argsObj;
            argsObj.SetObject();
            doc.AddMember("args", argsObj, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);
        do {
            HTTPResponse response = fetch("/%3F?%3F=%3F");
            const ByteArray *buffer = response.getBody();
            BOOST_REQUIRE_NE(buffer, static_cast<const ByteArray *>(nullptr));
            std::string body((const char *)buffer->data(), buffer->size());
            Document json;
            json.Parse(body.c_str());

            Document doc;
            Document::AllocatorType &a = doc.GetAllocator();
            doc.SetObject();
            doc.AddMember("path", "?", a);
            Value argsObj;
            argsObj.SetObject();
            Value values;
            values.SetArray();
            values.PushBack("?", a);
            argsObj.AddMember("?", values, a);
            doc.AddMember("args", argsObj, a);
            BOOST_CHECK_EQUAL(json, doc);
        } while (false);
    }
};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

//BOOST_FIXTURE_TEST_CASE(TestConnectionClose, TestCaseFixture<ConnectionCloseTest>) {
//    testCase.testConnectionClose();
//}

BOOST_FIXTURE_TEST_CASE(TestRequestEncoding, TestCaseFixture<RequestEncodingTest>) {
    testCase.testQuestionMark();
}

