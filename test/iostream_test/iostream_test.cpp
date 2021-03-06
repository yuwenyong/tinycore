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

    void onGet(const StringVector &args) override {
        write("Hello");
    }
};


template<typename BaseT>
class TestIOStreamWebMixin: public BaseT {
public:
    std::unique_ptr<Application> getApp() const override {
        Application::HandlersType handlers = {
                url<HelloHandler>("/"),
        };
        return make_unique<Application>(std::move(handlers));
    }

    void testConnectionClosed() {
        HTTPHeaders headers;
        headers["Connection"] = "close";
        HTTPResponse response = this->fetch("/", ARG_headers=headers);
        response.rethrow();
    }

    void testReadUntilClose() {
        auto stream = this->makeClientIOStream();
        stream->connect("localhost", this->getHTTPPort(), [this]() {
            this->stop();
        });
        this->wait();
        const char *line = "GET / HTTP/1.0\r\n\r\n";
        stream->write((const Byte*)line, strlen(line));
        stream->readUntilClose([this](ByteArray data) {
            this->stop(std::move(data));
        });
        ByteArray bytes = this->template wait<ByteArray>();
        std::string readString((char *)bytes.data(), bytes.size());
        BOOST_CHECK(boost::starts_with(readString, "HTTP/1.0 200"));
        BOOST_CHECK(boost::ends_with(readString, "Hello"));
    }

    void testReadZeroBytes() {
        auto stream = this->makeClientIOStream();
        stream->connect("localhost", this->getHTTPPort(), [this] {
            this->stop();
        });
        this->wait();
        const char * data = "GET / HTTP/1.0\r\n\r\n";
        stream->write((const Byte *)data, strlen(data));

        // normal read
        do  {
            stream->readBytes(9, [this](ByteArray d){
                this->stop(std::move(d));
            });
            ByteArray bytes = this->template wait<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "HTTP/1.0 ");
        } while (false);

        // zero bytes
        do {
            stream->readBytes(0, [this](ByteArray d){
                this->stop(std::move(d));
            });
            ByteArray bytes = this->template wait<ByteArray>();
            BOOST_REQUIRE_EQUAL(bytes.size(), 0);
        } while (false);

        // another normal read
        do {
            stream->readBytes(3, [this](ByteArray d){
                this->stop(std::move(d));
            });
            ByteArray bytes = this->template wait<ByteArray>();
            std::string readString((char *)bytes.data(), bytes.size());
            BOOST_REQUIRE_EQUAL(readString, "200");
        } while (false);

        stream->close();
    }

    void testWriteWhileConnecting() {
        auto stream = this->makeClientIOStream();
        std::vector<bool> connected(1, false);
        stream->connect("localhost", this->getHTTPPort(), [this, &connected]() {
            connected[0] = true;
            this->stop();
        });

        std::vector<bool> written(1, false);
        const char * line = "GET / HTTP/1.0\r\nConnection: close\r\n\r\n";
        stream->write((const Byte *)line, strlen(line), [this, &written]() {
            written[0] = true;
            this->stop();
        });
        BOOST_CHECK(!connected[0]);
        this->wait(5.0f, [&connected, &written]()->bool {
            return connected[0] && written[0];
        });
        stream->readUntilClose([this](ByteArray data) {
            this->stop(std::move(data));
        });
        ByteArray bytes = this->template wait<ByteArray>();
        std::string data((char *)bytes.data(), bytes.size());
        BOOST_CHECK(boost::ends_with(data, "Hello"));
    }
};


template <typename BaseT>
class TestIOStreamMixin: public BaseT {
public:
    using IOStreamPair = std::pair<std::shared_ptr<BaseIOStream>, std::shared_ptr<BaseIOStream>>;

    IOStreamPair makeIOStreamPair(size_t readChunkSize=0) {

        class _Server: public TCPServer {
        public:
            _Server(IOLoop *ioloop, std::shared_ptr<SSLOption> sslOption, AsyncTestCase *test, IOStreamPair &streams)
                    : TCPServer(ioloop, std::move(sslOption))
                    , _test(test)
                    , _streams(streams) {

            }

            void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) override {
                _streams.first = std::move(stream);
                _test->stop();
            }

        protected:
            AsyncTestCase *_test;
            IOStreamPair &_streams;
        };

        IOStreamPair streams;
        auto server = std::make_shared<_Server>(this->ioloop(), this->getServerSSLOption(), this, streams);
        server->listen(0);
        auto port = server->getLocalPort();
        auto clientStream = this->makeClientIOStream(readChunkSize);
        clientStream->connect("127.0.0.1", port, [this, &clientStream, &streams] {
            streams.second = std::move(clientStream);
            this->stop();
        });
        this->wait(5, [&streams]() {
            return streams.first && streams.second;
        });
        server->stop();
        return streams;
    }

    void testStreamingCallbackWithDataInBuffer() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        const char *line = "abcd\r\nefgh";
        client->write((const Byte *)line, strlen(line));
        server->readUntil("\r\n", [this](ByteArray data) {
            this->stop(std::move(data));
        });
        ByteArray data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(String::toString(data), "abcd\r\n");
        server->readUntilClose([this](ByteArray) {
            BOOST_CHECK(false);
        }, [this](ByteArray data) {
           this->stop(std::move(data));
        });
//        this->ioloop()->addTimeout(0.01f, [this]() {
//            this->stop();
//        });
        data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(String::toString(data), "efgh");
        server->close();
        client->close();
    }

    void testWriteZeroBytes() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        server->write(nullptr, 0, [this]() {
            this->stop();
        });
        this->wait();
        BOOST_CHECK(!server->writing());

        server->close();
        client->close();
    }

    void testConnectionRefused() {
        bool connectCalled = false;
        BaseIOStream::SocketType socket(this->ioloop()->getService());
        std::shared_ptr<IOStream> stream = IOStream::create(std::move(socket), this->ioloop());
        stream->setCloseCallback([this](){
            this->stop();
        });
        stream->connect("localhost", this->getUnusedPort(), [&connectCalled](){
            connectCalled = true;
        });
        this->wait();
        BOOST_CHECK(!connectCalled);
        BOOST_CHECK(stream->getError());
    }

    void testGaierror() {
        BaseIOStream::SocketType socket(this->ioloop()->getService());
        std::shared_ptr<IOStream> stream = IOStream::create(std::move(socket), this->ioloop());
        stream->setCloseCallback([this](){
            this->stop();
        });
        stream->connect("an invalid domain", 54321);
        this->wait();
        BOOST_CHECK(stream->getError());
    }

    void testReadCallbackError() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        server->setCloseCallback([this]() {
            this->stop();
        });
        {
            NullContext ctx;
            server->readBytes(1, [](ByteArray data) {
               ThrowException(ZeroDivisionError, "");
            });
        }
        const char *line = "1";
        client->write((const Byte *)line, strlen(line));
        this->wait();
        BOOST_CHECK(server->getError());
        try {
            std::rethrow_exception(server->getError());
        } catch (ZeroDivisionError &e) {

        } catch (...) {
            BOOST_CHECK(false);
        }
        server->close();
        client->close();
    }

    void testStreamingCallback() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        std::vector<ByteArray> chunks, targetChunks;
        std::vector<bool> finalCalled;
        server->readBytes(6, [this, &finalCalled] (ByteArray data) {
            BOOST_CHECK(data.empty());
            finalCalled.push_back(true);
            this->stop();
        }, [this, &chunks] (ByteArray data) {
            chunks.emplace_back(std::move(data));
            this->stop();
        });
        const char *data1 = "1234", *data2 = "5678", *data3 = "56";
        client->write((const Byte *)data1, strlen(data1));
        this->wait(5, [&chunks]() {
            return !chunks.empty();
        });
        client->write((const Byte *)data2, strlen(data2));
        this->wait(5, [&finalCalled]() {
            return !finalCalled.empty();
        });
        targetChunks.emplace_back((const Byte *)data1, (const Byte *)data1 + strlen(data1));
        targetChunks.emplace_back((const Byte *)data3, (const Byte *)data3 + strlen(data3));
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        server->readBytes(2, [this](ByteArray data) {
            this->stop(std::move(data));
        });
        ByteArray data = this->template wait<ByteArray>();
        ByteArray targetData {'7', '8'};
        BOOST_CHECK_EQUAL(data, targetData);
        server->close();
        client->close();
    }

    void testStreamingUntilClose() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        std::vector<ByteArray> chunks, targetChunks;
        std::array<bool, 1> closed{false};
        client->readUntilClose([this, &chunks, &closed](ByteArray data) {
//            chunks.emplace_back(std::move(data));
            ASSERT(data.empty());
            closed[0] = true;
            this->stop();
        }, [this, &chunks](ByteArray data) {
            chunks.emplace_back(std::move(data));
            this->stop();
        });
        const char *data1 = "1234", *data2 = "5678";
        server->write((const Byte *)data1, strlen(data1));
        this->wait(boost::none, [&chunks]() {
            return chunks.size() == 1;
        });
        server->write((const Byte *)data2, strlen(data2), [this]() {
            this->stop();
        });
        this->wait();
        server->close();
        this->wait(boost::none, [&closed]() {
            return closed[0];
        });
        targetChunks.emplace_back((const Byte *)data1, (const Byte *)data1 + strlen(data1));
        targetChunks.emplace_back((const Byte *)data2, (const Byte *)data2 + strlen(data2));
//        targetChunks.emplace_back();
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        client->close();
    }

    void testDelayedCloseCallback() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        client->setCloseCallback([this]() {
            this->stop();
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
        this->wait();
        targetChunks.emplace_back(1, (Byte)'1');
        targetChunks.emplace_back(1, (Byte)'2');
        BOOST_CHECK_EQUAL(chunks, targetChunks);
        client->close();
    }

    void testCloseBufferedData() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair(256);
        ByteArray data(512, (Byte)'A');
        server->write(data.data(), data.size());
        client->readBytes(256, [this](ByteArray data) {
            this->stop(std::move(data));
        });
        data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(data, ByteArray(256, (Byte)'A'));
        server->close();
        this->ioloop()->addTimeout(0.01f, [this]() {
            this->stop();
        });
        this->wait();
        client->readBytes(256, [this](ByteArray data) {
            this->stop(std::move(data));
        });
        data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(data, ByteArray(256, (Byte)'A'));
        client->close();
    }

    void testReadUntilCloseAfterClose() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        client->setCloseCallback([this]() {
            this->stop();
        });
        const char *line = "1234";
        server->write((const Byte *)line, strlen(line), [&server] () {
            server->close();
        });
//        server->close();
        this->wait(boost::none);
        client->readUntilClose([this](ByteArray data) {
            this->stop(std::move(data));
        });
        ByteArray data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(String::toString(data), line);
        server->close();
        client->close();
    }

    void testStreamingReadUntilCloseAfterClose() {
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        client->setCloseCallback([this]() {
            this->stop();
        });
        const char *line = "1234";
        server->write((const Byte *)line, strlen(line), [&server](){
            server->close();
        });
//        server->close();
        this->wait();
        std::vector<ByteArray> streamingChunks;
        client->readUntilClose([this](ByteArray data) {
            this->stop(std::move(data));
        }, [this, &streamingChunks](ByteArray data) {
            streamingChunks.push_back(std::move(data));
        });
        ByteArray data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(data.size(), 0);
        std::string streamingData;
        for (auto &streamingChunk: streamingChunks) {
            streamingData.append((const char *)streamingChunk.data(), streamingChunk.size());
        }
        BOOST_CHECK_EQUAL(streamingData, line);
        server->close();
        client->close();
    }

    void testLargeReadUntil() {
        Timestamp start = TimestampClock::now();
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();

        ByteArray data(1024, (Byte)'A');
        for (size_t i = 0; i < 4096; ++i) {
            client->write(data.data(), data.size());
        }
        const char *end = "\r\n";
        client->write((const Byte *)end, strlen(end));
        server->readUntil(end, [this](ByteArray data) {
            this->stop(std::move(data));
        });
        data = this->template wait<ByteArray>();
        BOOST_CHECK_EQUAL(data.size(), 1024 * 4096 + 2);
        server->close();
        client->close();
        auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(TimestampClock::now() - start);
        double cost = elapse.count() / 1000000 + elapse.count() % 1000000 / 1000000.0;
        std::cerr << "Cost " << cost << "s" << std::endl;
    }

    void testCloseCallbackWithPendingRead() {
        const char *ok = "OK\r\n";
        std::shared_ptr<BaseIOStream> server, client;
        std::tie(server, client) = makeIOStreamPair();
        client->setCloseCallback([this]() {
            this->stop();
        });
        server->write((const Byte *)ok, strlen(ok));
        client->readUntil("\r\n", [this](ByteArray data) {
            this->stop(std::move(data));
        });
        std::string res = String::toString(this->template wait<ByteArray>());
        BOOST_CHECK_EQUAL(res, ok);
        server->close();
        client->readUntil("\r\n", [](ByteArray data) {});
        auto result = this->wait();
        BOOST_CHECK(result.empty());
        server->close();
        client->close();
    }
};


class TestIOStreamWebHTTPImpl: public AsyncHTTPTestCase {
protected:
    std::shared_ptr<BaseIOStream> makeClientIOStream() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        return IOStream::create(std::move(socket), &_ioloop);
    }
};


class TestIOStreamWebHTTPSImpl: public AsyncHTTPSTestCase {
public:
    std::shared_ptr<BaseIOStream> makeClientIOStream() {
        BaseIOStream::SocketType socket(_ioloop.getService());
        SSLParams sslParams(false);
        sslParams.setVerifyMode(SSLVerifyMode::CERT_NONE);
        auto sslOption = SSLOption::create(sslParams);
        return SSLIOStream::create(std::move(socket), std::move(sslOption), &_ioloop);
    }
};

using TestIOStreamWebHTTP = TestIOStreamWebMixin<TestIOStreamWebHTTPImpl>;
using TestIOStreamWebHTTPS = TestIOStreamWebMixin<TestIOStreamWebHTTPSImpl>;


class TestIOStreamImpl: public AsyncTestCase {
public:
    std::shared_ptr<BaseIOStream> makeClientIOStream(size_t readChunkSize=0) {
        BaseIOStream::SocketType socket(_ioloop.getService());
        return IOStream::create(std::move(socket), &_ioloop, DEFAULT_MAX_BUFFER_SIZE,
                                readChunkSize != 0 ? readChunkSize : DEFAULT_READ_CHUNK_SIZE);
    }

    std::shared_ptr<SSLOption> getServerSSLOption() const {
        return nullptr;
    }
};


class TestIOStreamSSLImpl: public AsyncTestCase {
public:
    std::shared_ptr<BaseIOStream> makeClientIOStream(size_t readChunkSize=0) {
        BaseIOStream::SocketType socket(_ioloop.getService());
        SSLParams sslParams(false);
        sslParams.setVerifyMode(SSLVerifyMode::CERT_NONE);
        auto sslOption = SSLOption::create(sslParams);
        return SSLIOStream::create(std::move(socket), std::move(sslOption), &_ioloop, DEFAULT_MAX_BUFFER_SIZE,
                                   readChunkSize != 0 ? readChunkSize : DEFAULT_READ_CHUNK_SIZE);
    }

    std::shared_ptr<SSLOption> getServerSSLOption() const {
        SSLParams sslParams(true);
        sslParams.setKeyFile("test.key");
        sslParams.setCertFile("test.crt");
        auto sslOption = SSLOption::create(sslParams);
        return sslOption;
    }
};


using TestIOStream = TestIOStreamMixin<TestIOStreamImpl>;
using TestIOStreamSSL = TestIOStreamMixin<TestIOStreamSSLImpl>;


TINYCORE_TEST_INIT()
TINYCORE_TEST_CASE(TestIOStreamWebHTTP, testConnectionClosed)
TINYCORE_TEST_CASE(TestIOStreamWebHTTP, testReadUntilClose)
TINYCORE_TEST_CASE(TestIOStreamWebHTTP, testReadZeroBytes)
TINYCORE_TEST_CASE(TestIOStreamWebHTTP, testWriteWhileConnecting)

TINYCORE_TEST_CASE(TestIOStreamWebHTTPS, testConnectionClosed)
TINYCORE_TEST_CASE(TestIOStreamWebHTTPS, testReadUntilClose)
TINYCORE_TEST_CASE(TestIOStreamWebHTTPS, testReadZeroBytes)
TINYCORE_TEST_CASE(TestIOStreamWebHTTPS, testWriteWhileConnecting)

TINYCORE_TEST_CASE(TestIOStream, testStreamingCallbackWithDataInBuffer)
TINYCORE_TEST_CASE(TestIOStream, testWriteZeroBytes)
TINYCORE_TEST_CASE(TestIOStream, testConnectionRefused)
TINYCORE_TEST_CASE(TestIOStream, testGaierror)
TINYCORE_TEST_CASE(TestIOStream, testReadCallbackError)
TINYCORE_TEST_CASE(TestIOStream, testStreamingCallback)
TINYCORE_TEST_CASE(TestIOStream, testStreamingUntilClose)
TINYCORE_TEST_CASE(TestIOStream, testDelayedCloseCallback)
TINYCORE_TEST_CASE(TestIOStream, testCloseBufferedData)
TINYCORE_TEST_CASE(TestIOStream, testReadUntilCloseAfterClose)
TINYCORE_TEST_CASE(TestIOStream, testStreamingReadUntilCloseAfterClose)
TINYCORE_TEST_CASE(TestIOStream, testLargeReadUntil)
TINYCORE_TEST_CASE(TestIOStream, testCloseCallbackWithPendingRead)

TINYCORE_TEST_CASE(TestIOStreamSSL, testStreamingCallbackWithDataInBuffer)
TINYCORE_TEST_CASE(TestIOStreamSSL, testWriteZeroBytes)
TINYCORE_TEST_CASE(TestIOStreamSSL, testConnectionRefused)
TINYCORE_TEST_CASE(TestIOStreamSSL, testGaierror)
TINYCORE_TEST_CASE(TestIOStreamSSL, testReadCallbackError)
TINYCORE_TEST_CASE(TestIOStreamSSL, testStreamingCallback)
TINYCORE_TEST_CASE(TestIOStreamSSL, testStreamingUntilClose)
TINYCORE_TEST_CASE(TestIOStreamSSL, testDelayedCloseCallback)
TINYCORE_TEST_CASE(TestIOStreamSSL, testCloseBufferedData)
TINYCORE_TEST_CASE(TestIOStreamSSL, testReadUntilCloseAfterClose)
TINYCORE_TEST_CASE(TestIOStreamSSL, testStreamingReadUntilCloseAfterClose)
TINYCORE_TEST_CASE(TestIOStreamSSL, testLargeReadUntil)
TINYCORE_TEST_CASE(TestIOStreamSSL, testCloseCallbackWithPendingRead)


