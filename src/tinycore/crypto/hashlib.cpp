//
// Created by yuwenyong on 17-4-28.
//

#include "tinycore/crypto/hashlib.h"
#include "tinycore/debugging/trace.h"

class OpenSSLInit {
public:
    OpenSSLInit() {
        OpenSSL_add_all_digests();
        ERR_load_crypto_strings();
    }

    ~OpenSSLInit() {

    }
};


OpenSSLInit gInit;


EVPContext::EVPContext(const char *name) {
    const EVP_MD *type = EVP_get_digestbyname(name);
    ASSERT(type != nullptr);
    _ctx = EVP_MD_CTX_new();
    EVP_DigestInit(_ctx, type);
}


EVPContext::~EVPContext() {
    if (_ctx) {
        EVP_MD_CTX_free(_ctx);
        _ctx = nullptr;
    }
}

