//
// Created by yuwenyong on 17-4-28.
//

#ifndef TINYCORE_HASHLIB_H
#define TINYCORE_HASHLIB_H

#include "tinycore/common/common.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <boost/mpl/string.hpp>


#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x00908000)
#define _OPENSSL_SUPPORTS_SHA2
#endif


#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif


class TC_COMMON_API EVPContext {
public:
    EVPContext(const char *name);
    ~EVPContext();

    const EVP_MD_CTX* native() const {
        return _ctx;
    }
protected:
    EVP_MD_CTX *_ctx{nullptr};
};


template <typename T>
class EVPObject {
public:
    typedef T HashNameType;

    EVPObject() {
        _ctx = EVP_MD_CTX_new();
        EVP_MD_CTX_copy(_ctx, initializer().native());
    }

    EVPObject(const void *d, size_t cnt): EVPObject() {
        update(d, cnt);
    }

    EVPObject(const char *s): EVPObject() {
        update(s);
    }

    EVPObject(const EVPObject &rhs) {
        _ctx = EVP_MD_CTX_new();
        EVP_MD_CTX_copy(_ctx, rhs._ctx);
    }

    EVPObject(EVPObject &&rhs) {
        _ctx = rhs._ctx;
        rhs._ctx = nullptr;
    }

    EVPObject& operator=(const EVPObject &rhs) {
        reset();
        _ctx = EVP_MD_CTX_new();
        EVP_MD_CTX_copy(_ctx, rhs._ctx);
        return *this;
    }

    EVPObject& operator=(EVPObject &&rhs) {
        reset();
        _ctx = rhs._ctx;
        rhs._ctx = nullptr;
        return *this;
    }

    ~EVPObject() {
        reset();
    }

    void update(const char *s) {
        EVP_DigestUpdate(_ctx, s, strlen(s));
    }

    void update(const void *d, size_t cnt) {
        EVP_DigestUpdate(_ctx, d, cnt);
    }

    int digestSize() const {
        return EVP_MD_CTX_size(_ctx);
    }

    int blockSize() const {
        return EVP_MD_CTX_block_size(_ctx);
    }

    std::string digest() const {
        unsigned char digest[EVP_MAX_MD_SIZE];
        std::string result;
        unsigned int digestSize;
        EVPObject temp(*this);
        digestSize = (unsigned int)temp.digestSize();
        EVP_DigestFinal(temp._ctx, digest, NULL);
        result.assign((char *)digest, digestSize);
        return result;
    }

    std::string hex() const {
        unsigned char digest[EVP_MAX_MD_SIZE];
        std::string result;
        unsigned int digestSize;
        EVPObject temp(*this);
        digestSize = (unsigned int)temp.digestSize();
        EVP_DigestFinal(temp._ctx, digest, NULL);
        char c;
        for(unsigned int i = 0; i < digestSize; i++) {
            c = (digest[i] >> 4) & 0xf;
            c = (c > 9) ? c +'a'-10 : c + '0';
            result.push_back(c);
            c = (digest[i] & 0xf);
            c = (c > 9) ? c +'a'-10 : c + '0';
            result.push_back(c);
        }
        return result;
    }
protected:
    void reset() {
        if (_ctx) {
            EVP_MD_CTX_free(_ctx);
            _ctx = nullptr;
        }
    }

    static const EVPContext& initializer() {
        static const EVPContext context(boost::mpl::c_str<HashNameType>::value);
        return context;
    }

    EVP_MD_CTX *_ctx{nullptr};
};


typedef EVPObject<boost::mpl::string<'m', 'd', '5'>> MD5Object;
typedef EVPObject<boost::mpl::string<'s', 'h', 'a', '1'>> SHA1Object;
#ifdef _OPENSSL_SUPPORTS_SHA2
typedef EVPObject<boost::mpl::string<'s', 'h', 'a', '2', '2', '4'>> SHA224Object;
typedef EVPObject<boost::mpl::string<'s', 'h', 'a', '2', '5', '6'>> SHA256Object;
typedef EVPObject<boost::mpl::string<'s', 'h', 'a', '3', '8', '4'>> SHA384Object;
typedef EVPObject<boost::mpl::string<'s', 'h', 'a', '5', '1', '2'>> SHA512Object;
#endif


#endif //TINYCORE_HASHLIB_H
