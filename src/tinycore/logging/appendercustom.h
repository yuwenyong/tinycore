//
// Created by yuwenyong on 17-3-9.
//

#ifndef TINYCORE_APPENDERCUSTOM_H
#define TINYCORE_APPENDERCUSTOM_H

#include "tinycore/logging/appender.h"


template <typename Sink>
class AppenderCustom: public Appender {
public:
    typedef typename Sink::AppenderType AppenderType;

    AppenderCustom(std::string name, LogLevel level, AppenderFlags flags, const StringVector &extraArgs)
            : Appender(name, level, flags)
            , _extraArgs(extraArgs) {

    }

protected:
    SinkTypePtr createSink() const override {
        using SinkBackend = Sink;
        using SinkFrontend = sinks::synchronous_sink<SinkBackend>;
        auto backend = boost::make_shared<SinkBackend >(_extraArgs);
        auto sink = boost::make_shared<SinkFrontend>(backend);
        return sink;
    }

    StringVector _extraArgs;
};


class BaseSink : public sinks::basic_formatted_sink_backend<
        char,
        sinks::synchronized_feeding
> {
public:
    BaseSink() = default;
    virtual ~BaseSink();

    void consume(const logging::record_view &rec, const std::string &format) {
        const DateTime *timeStamp = rec[attr_timestamp].get_ptr();
        LogLevel logLevel = rec[attr_severity].get();
        const std::string *channel = rec[attr_channel].get_ptr();
        const std::string *message = rec[expr::smessage].get_ptr();
        onConsume(*timeStamp, logLevel, *channel, *message, format);
    }


    virtual void onConsume(const DateTime &timeStamp, LogLevel logLevel, const std::string &channel,
                           const std::string &message, const std::string &format) = 0;
};


#endif //TINYCORE_APPENDERCUSTOM_H
