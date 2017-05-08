//
// Created by yuwenyong on 17-5-5.
//

#ifndef TINYCORE_WEBSOCKET_H
#define TINYCORE_WEBSOCKET_H

#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"
#include "tinycore/networking/web.h"

//
//class TC_COMMON_API WebSocketHandler: public RequestHandler {
//public:
//    using RequestHandler::RequestHandler;
//
//protected:
//
//public:
//    void write(const char *chunk, size_t length);
//    void redirect(const std::string &url, bool permanent=false);
//    void setHeader(const std::string &name, const std::string &value);
//    void sendError(int statusCode = 500);
//    void setCookie(const std::string &name, const std::string &value, const char *domain= nullptr,
//                   const DateTime *expires= nullptr, const char *path= "/", int *expiresDays= nullptr,
//                   const StringMap *args= nullptr);
//    void setStatus(int statusCode);
//    void flush(bool includeFooters= false);
//    void finish();
//};

#endif //TINYCORE_WEBSOCKET_H
