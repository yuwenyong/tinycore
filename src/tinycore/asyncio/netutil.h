//
// Created by yuwenyong on 17-6-7.
//

#ifndef TINYCORE_NETUTIL_H
#define TINYCORE_NETUTIL_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "tinycore/asyncio/iostream.h"


class TC_COMMON_API TCPServer: public std::enable_shared_from_this<TCPServer> {
public:
    typedef boost::asio::ip::tcp::acceptor AcceptorType;

    TCPServer(IOLoop *ioloop = nullptr, std::shared_ptr<SSLOption> sslOption = nullptr);
    virtual ~TCPServer();
    TCPServer(const TCPServer &) = delete;
    TCPServer &operator=(const TCPServer &) = delete;
    void listen(unsigned short port, std::string address = "::");

    void bind(unsigned short port, std::string address);

    void start() {
        doAccept();
    }

    void stop();

    virtual void handleStream(std::shared_ptr<BaseIOStream> stream, std::string address) = 0;
protected:
    void doAccept() {
        _acceptor.async_accept(_socket, std::bind(&TCPServer::onAccept, shared_from_this(), std::placeholders::_1));
    }

    void onAccept(const boost::system::error_code &ec);

    IOLoop *_ioloop;
    std::shared_ptr<SSLOption> _sslOption;
    AcceptorType _acceptor;
    BaseIOStream::SocketType _socket;
};


#endif //TINYCORE_NETUTIL_H
