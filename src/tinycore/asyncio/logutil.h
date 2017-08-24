//
// Created by yuwenyong on 17-8-12.
//

#ifndef TINYCORE_LOGUTIL_H
#define TINYCORE_LOGUTIL_H

#include "tinycore/common/common.h"
#include "tinycore/configuration/options.h"
#include "tinycore/logging/logging.h"


#define SYS_TIMEOUT_COUNT                   "TinyCore.Timeout.Count"
#define SYS_PERIODICCALLBACK_COUNT          "TinyCore.PeriodicCallback.Count"
#define SYS_IOSTREAM_COUNT                  "TinyCore.IOStream.Count"
#define SYS_SSLIOSTREAM_COUNT               "TinyCore.SSLIOStream.Count"
#define SYS_HTTPCONNECTION_COUNT            "TinyCore.HTTPConnection.Count"
#define SYS_HTTPSERVERREQUEST_COUNT         "TinyCore.HTTPServerRequest.Count"
#define SYS_REQUESTHANDLER_COUNT            "TinyCore.RequestHandler.Count"
#define SYS_WEBSOCKETHANDLER_COUNT          "TinyCore.WebSocketHandler.Count"
#define SYS_HTTPCLIENT_COUNT                "TinyCore.HTTPClient.Count"
#define SYS_HTTPCLIENTCONNECTION_COUNT      "TinyCore.HTTPClientConnection.Count"
#define SYS_WEBSOCKETCLIENTCONNECTION_COUNT "TinyCore.WebSocketClientConnection.Count"


extern Logger *gAccessLog;
extern Logger *gAppLog;
extern Logger *gGenLog;


class LogUtil {
public:
    static void initGlobalLoggers();

    static void enablePrettyLogging(const OptionParser *options);

    static void defineLoggingOptions(OptionParser *options);
};


#endif //TINYCORE_LOGUTIL_H
