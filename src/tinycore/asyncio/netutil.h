//
// Created by yuwenyong on 17-6-7.
//

#ifndef TINYCORE_NETUTIL_H
#define TINYCORE_NETUTIL_H

#include "tinycore/common/common.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>


enum class SSLVerifyMode {
    CERT_NONE,
    CERT_OPTIONAL,
    CERT_REQUIRED,
};


class SSLParams {
public:
    explicit SSLParams(bool serverSide)
            : _serverSide(serverSide) {
    }

    void setCertFile(const std::string &certFile) {
        _certFile = certFile;
    }

    const std::string& getCertFile() const {
        return _certFile;
    }

    void setKeyFile(const std::string &keyFile) {
        _keyFile = keyFile;
    }

    const std::string& getKeyFile() const {
        return _keyFile;
    }

    void setPassword(const std::string &password) {
        _password = password;
    }

    const std::string& getPassword() const {
        return _password;
    }

    void setVerifyMode(SSLVerifyMode verifyMode) {
        _verifyMode = verifyMode;
    }

    SSLVerifyMode getVerifyMode() const {
        return _verifyMode;
    }

    void setVerifyFile(const std::string &verifyFile) {
        _verifyFile = verifyFile;
    }

    const std::string& getVerifyFile() const {
        return _verifyFile;
    }

    void setCheckHost(const std::string &hostName) {
        _checkHost = hostName;
    }

    const std::string& getCheckHost() const {
        return _checkHost;
    }

    bool isServerSide() const {
        return _serverSide;
    }

protected:
    bool _serverSide;
    SSLVerifyMode _verifyMode{SSLVerifyMode::CERT_NONE};
    std::string _certFile;
    std::string _keyFile;
    std::string _password;
    std::string _verifyFile;
    std::string _checkHost;
};


class SSLOption: public boost::noncopyable {
public:
    typedef boost::asio::ssl::context SSLContextType;

    explicit SSLOption(const SSLParams &sslParams);

    bool isServerSide() const {
        return _serverSide;
    }

    SSLContextType &context() {
        return _context;
    }

    static std::shared_ptr<SSLOption> create(const SSLParams &sslParams);
protected:
    void setCertFile(const std::string &certFile) {
        _context.use_certificate_chain_file(certFile);
    }

    void setKeyFile(const std::string &keyFile) {
        _context.use_private_key_file(keyFile, boost::asio::ssl::context::pem);
    }

    void setPassword(const std::string &password) {
        _context.set_password_callback([password](size_t, boost::asio::ssl::context::password_purpose) {
            return password;
        });
    }

    void setVerifyMode(SSLVerifyMode verifyMode) {
        if (verifyMode == SSLVerifyMode::CERT_NONE) {
            _context.set_verify_mode(boost::asio::ssl::verify_none);
        } else if (verifyMode == SSLVerifyMode::CERT_OPTIONAL) {
            _context.set_verify_mode(boost::asio::ssl::verify_peer);
        } else {
            _context.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
        }
    }

    void setVerifyFile(const std::string &verifyFile) {
        _context.load_verify_file(verifyFile);
    }

    void setDefaultVerifyPath() {
        _context.set_default_verify_paths();
    }

    void setCheckHost(const std::string &hostName) {
        _context.set_verify_callback(boost::asio::ssl::rfc2818_verification(hostName));
    }

    bool _serverSide;
    SSLContextType _context;
};


#endif //TINYCORE_NETUTIL_H
