//
// Created by yuwenyong on 17-6-7.
//

#include "tinycore/asyncio/netutil.h"
#include <boost/filesystem.hpp>
#include "tinycore/common/errors.h"


SSLOption::SSLOption(const SSLParams &sslParams)
        : _serverSide(sslParams.isServerSide())
        , _context(boost::asio::ssl::context::sslv23) {
    boost::system::error_code ec;
    _context.set_options(boost::asio::ssl::context::no_sslv3, ec);
    const std::string &certFile = sslParams.getCertFile();
    if (!certFile.empty()) {
        setCertFile(certFile);
    }
    const std::string &keyFile = sslParams.getKeyFile();
    if (!keyFile.empty()) {
        setKeyFile(keyFile);
    }
    const std::string &password = sslParams.getPassword();
    if (!password.empty()) {
        setPassword(password);
    }
    if (!_serverSide) {
        auto verifyMode = sslParams.getVerifyMode();
        setVerifyMode(verifyMode);
        const std::string &verifyFile = sslParams.getVerifyFile();
        if (!verifyFile.empty()) {
            setVerifyFile(verifyFile);
        } else if(verifyMode != SSLVerifyMode::CERT_NONE) {
            setDefaultVerifyPath();
        }
        const std::string &checkHost = sslParams.getCheckHost();
        if (!checkHost.empty()) {
            setCheckHost(checkHost);
        }
    }
}

std::shared_ptr<SSLOption> SSLOption::create(const SSLParams &sslParams) {
    if (sslParams.isServerSide()) {
        if (sslParams.getCertFile().empty()) {
            ThrowException(KeyError, "missing cert file in sslParams");
        }
    } else {
        if (sslParams.getVerifyMode() != SSLVerifyMode::CERT_NONE && sslParams.getVerifyFile().empty()) {
            ThrowException(KeyError, "missing verify file in sslParams");
        }
    }
    const std::string &certFile = sslParams.getCertFile();
    if (!certFile.empty() && !boost::filesystem::exists(certFile)) {
        ThrowException(ValueError, "cert file \"%s\" does not exist", certFile.c_str());
    }
    const std::string &keyFile = sslParams.getKeyFile();
    if (!keyFile.empty() && !boost::filesystem::exists(keyFile)) {
        ThrowException(ValueError, "key file \"%s\" does not exist", certFile.c_str());
    }
    const std::string &verifyFile = sslParams.getVerifyFile();
    if (!verifyFile.empty() && !boost::filesystem::exists(verifyFile)) {
        ThrowException(ValueError, "verify file \"%s\" does not exist", certFile.c_str());
    }
    auto sslOption = std::make_shared<SSLOption>(sslParams);
    return sslOption;
}