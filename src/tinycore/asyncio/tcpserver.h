//
// Created by yuwenyong on 17-8-14.
//

#ifndef TINYCORE_TCPSERVER_H
#define TINYCORE_TCPSERVER_H

#include "tinycore/common/common.h"
#include "tinycore/asyncio/iostream.h"
#include "tinycore/asyncio/netutil.h"


class TC_COMMON_API TCPServer: public std::enable_shared_from_this<TCPServer> {
public:
    typedef boost::asio::ip::tcp::acceptor AcceptorType;

    explicit TCPServer(IOLoop *ioloop = nullptr, std::shared_ptr<SSLOption> sslOption = nullptr, size_t maxBufferSize=0,
                       size_t readChunkSize=0);

    virtual ~TCPServer();

    TCPServer(const TCPServer &) = delete;

    TCPServer &operator=(const TCPServer &) = delete;

    void listen(unsigned short port, std::string address = "::");

    void bind(unsigned short port, std::string address);

    void start() {
        accept();
    }

    unsigned short getLocalPort() const {
        auto endpoint = _acceptor.local_endpoint();
        return endpoint.port();
    }

    void stop();

    virtual void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) = 0;
protected:
    void accept() {
        _acceptor.async_accept(_socket, std::bind(&TCPServer::onAccept, shared_from_this(), std::placeholders::_1));
    }

    void onAccept(const boost::system::error_code &ec);

    IOLoop *_ioloop;
    std::shared_ptr<SSLOption> _sslOption;
    AcceptorType _acceptor;
    BaseIOStream::SocketType _socket;
    size_t _maxBufferSize;
    size_t _readChunkSize;
};

#endif //TINYCORE_TCPSERVER_H
