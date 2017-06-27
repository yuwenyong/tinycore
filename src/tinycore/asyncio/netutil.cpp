//
// Created by yuwenyong on 17-6-7.
//

#include "tinycore/asyncio/netutil.h"
#include "tinycore/asyncio/ioloop.h"

TCPServer::TCPServer(IOLoop *ioloop, std::shared_ptr<SSLOption> sslOption)
        : _ioloop(ioloop ? ioloop : sIOLoop)
        , _sslOption(std::move(sslOption))
        , _acceptor(_ioloop->getService())
        , _socket(_ioloop->getService()){

}

TCPServer::~TCPServer() {

}

void TCPServer::listen(unsigned short port, std::string address) {
    bind(port, std::move(address));
    start();
}

void TCPServer::bind(unsigned short port, std::string address) {
    BaseIOStream::ResolverType resolver(_ioloop->getService());
    BaseIOStream::ResolverType::query query(address, std::to_string(port));
    BaseIOStream::EndPointType endpoint = *resolver.resolve(query);
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen();
}

void TCPServer::stop() {
    _acceptor.close();
}

void TCPServer::onAccept(const boost::system::error_code &ec) {
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            throw boost::system::system_error(ec);
        }
    } else {
        try {
            std::shared_ptr<BaseIOStream> stream;
            if (_sslOption) {
                stream = SSLIOStream::create(std::move(_socket), _sslOption, _ioloop);
            } else {
                stream = IOStream::create(std::move(_socket), _ioloop);
            }
            stream->start();
            std::string remoteAddress = stream->getRemoteAddress();
            handleStream(std::move(stream), std::move(remoteAddress));
        } catch (std::exception &e) {
            Log::error("Error in connection callback:%s", e.what());
        }
        doAccept();
    }
}