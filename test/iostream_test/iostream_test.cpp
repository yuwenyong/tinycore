//
// Created by yuwenyong on 17-5-27.
//

#define BOOST_TEST_MODULE iostream_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_TEST_DONT_PRINT_LOG_VALUE(ByteArray)
BOOST_TEST_DONT_PRINT_LOG_VALUE(std::vector<ByteArray>)


class HelloHandler: public RequestHandler {
public:
    using RequestHandler::RequestHandler;

    void onGet(StringVector args) override {
        write("Hello");
    }
};


class IOStreamTest: public AsyncHTTPTestCase {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloHandler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    using IOStreamPair = std::pair<std::shared_ptr<BaseIOStream>, std::shared_ptr<BaseIOStream>>;

    IOStreamPair makeIOStreamPair() {

        class _Server: public TCPServer {
        public:
            _Server(IOLoop *ioloop, AsyncHTTPTestCase *test, IOStreamPair &streams)
                    : TCPServer(ioloop)
                    , _test(test)
                    , _streams(streams) {

            }

            void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override {
//                Log::error("OnAccept");
                _streams.first = std::move(stream);
                _test->stop();
            }

        protected:
            AsyncHTTPTestCase *_test;
            IOStreamPair &_streams;
        };

        IOStreamPair streams;
        auto port = getUnusedPort();
        _Server server(&_ioloop, this, streams);
        server.listen(port);
        BaseIOStream::SocketType socket(_ioloop.getService());
        std::shared_ptr<IOStream> clientStream = IOStream::create(std::move(socket), &_ioloop);
        clientStream->connect("127.0.0.1", port, [this, &clientStream, &streams] {
//            Log::error("OnConnect");
            streams.second = std::move(clientStream);
            stop();
        });
        wait(5, [&streams]() {
            return streams.first && streams.second;
        });
        server.stop();
        return streams;
    }

    void testReadZeroBytes() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        std::shared_ptr<IOStream> stream = IOStream::create(std::move(socket), &_ioloop);
        stream->connect("localhost", getHTTPPort(), [this] {
            stop();
        });
        wait();
        const char * data = "GET / HTTP/1.0\r\n\r\n";
        stream->write((const Byte *)data, strlen(data));

        // normal read
        do  {
            stream->readBytes(9, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "HTTP/1.0 ");
        } while (false);

        // zero bytes
        do {
            stream->readBytes(0, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            BOOST_REQUIRE_EQUAL(bytes.size(), 0);
        } while (false);

        // another normal read
        do {
            stream->readBytes(3, [this](ByteArray d){
                stop(std::move(d));
            });
            ByteArray bytes = waitResult<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "200");
        } while (false);
    }

    void testWriteZeroBytes() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        server->write(nullptr, 0, [this]() {
            stop();
        });
        wait();
        BOOST_CHECK(!server->writing());
    }

    void testConnectionRefused() {
        bool connectCalled = false;
        BaseIOStream::SocketType socket(_ioloop.getService());
        std::shared_ptr<IOStream> stream = IOStream::create(std::move(socket), &_ioloop);
        stream->setCloseCallback([this](){
            stop();
        });
        stream->connect("localhost", getUnusedPort(), [&connectCalled](){
            connectCalled = true;
        });
        wait();
        BOOST_CHECK(!connectCalled);
    }

    void testConnectionClosed() {
        HTTPHeaders headers;
        headers["Connection"] = "close";
        HTTPResponse response = fetch("/", ARG_headers=headers);
        response.rethrow();
    }

    void testReadUntilClose() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        std::shared_ptr<IOStream> stream = IOStream::create(std::move(socket), &_ioloop);
        stream->connect("localhost", getHTTPPort(), [this]() {
            stop();
        });
        wait();
        const char *line = "GET / HTTP/1.0\r\n\r\n";
        stream->write((const Byte*)line, strlen(line));
        stream->readUntilClose([this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray bytes = waitResult<ByteArray>();
        std::string readString((char *)bytes.data(), bytes.size());
        BOOST_CHECK(boost::starts_with(readString, "HTTP/1.0 200"));
        BOOST_CHECK(boost::ends_with(readString, "Hello"));
    }

    void testStreamingCallback() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        std::vector<ByteArray> chunks, targetChunks;
        std::vector<bool> finalCalled;
        server->readBytes(6, [this, &finalCalled] (ByteArray data) {
            BOOST_CHECK(data.empty());
            finalCalled.push_back(true);
            stop();
        }, [this, &chunks] (ByteArray data) {
            chunks.emplace_back(std::move(data));
            stop();
        });
        const char *data1 = "1234", *data2 = "5678", *data3 = "56";
        client->write((const Byte *)data1, strlen(data1));
        wait(5, [&chunks]() {
            return !chunks.empty();
        });
        client->write((const Byte *)data2, strlen(data2));
        wait(5, [&finalCalled]() {
            return !finalCalled.empty();
        });
        targetChunks.emplace_back((const Byte *)data1, (const Byte *)data1 + strlen(data1));
        targetChunks.emplace_back((const Byte *)data3, (const Byte *)data3 + strlen(data3));
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        server->readBytes(2, [this](ByteArray data) {
            stop(std::move(data));
        });
        ByteArray data = waitResult<ByteArray>();
        ByteArray targetData {'7', '8'};
        BOOST_CHECK_EQUAL(data, targetData);
        server->close();
        client->close();
    }

    void testStreamingUntilClose() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        std::vector<ByteArray> chunks, targetChunks;
        client->readUntilClose([this, &chunks](ByteArray data) {
            chunks.emplace_back(std::move(data));
            stop();
        }, [this, &chunks](ByteArray data) {
            chunks.emplace_back(std::move(data));
            stop();
        });
        const char *data1 = "1234", *data2 = "5678";
        server->write((const Byte *)data1, strlen(data1));
        wait();
        server->write((const Byte *)data2, strlen(data2));
        wait();
        server->close();
        wait();
        targetChunks.emplace_back((const Byte *)data1, (const Byte *)data1 + strlen(data1));
        targetChunks.emplace_back((const Byte *)data2, (const Byte *)data2 + strlen(data2));
        targetChunks.emplace_back();
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        client->close();
    }

    void testDelayedCloseCallback() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        client->setCloseCallback([this]() {
            stop();
        });
        const char *data1 = "12";
        server->write((const Byte *)data1, strlen(data1));
        std::vector<ByteArray> chunks, targetChunks;
        client->readBytes(1, [&](ByteArray data) {
            chunks.emplace_back(std::move(data));
            client->readBytes(1, [&](ByteArray data) {
                chunks.emplace_back(std::move(data));
            });
            server->close();
        });
        wait();
        targetChunks.emplace_back(1, (Byte)'1');
        targetChunks.emplace_back(1, (Byte)'2');
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        client->close();
    }
};


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(IOStreamTest, testReadZeroBytes)
TINYCORE_TEST_CASE(IOStreamTest, testWriteZeroBytes)
TINYCORE_TEST_CASE(IOStreamTest, testConnectionRefused)
TINYCORE_TEST_CASE(IOStreamTest, testConnectionClosed)
TINYCORE_TEST_CASE(IOStreamTest, testReadUntilClose)
TINYCORE_TEST_CASE(IOStreamTest, testStreamingCallback)
TINYCORE_TEST_CASE(IOStreamTest, testStreamingUntilClose)
TINYCORE_TEST_CASE(IOStreamTest, testDelayedCloseCallback)
