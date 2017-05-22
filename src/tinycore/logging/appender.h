//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_APPENDER_H
#define TINYCORE_APPENDER_H

#include "tinycore/common/common.h"
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/functional/factory.hpp>
#include <boost/phoenix.hpp>
#include <boost/noncopyable.hpp>
#include "tinycore/logging/logger.h"


namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;


BOOST_LOG_ATTRIBUTE_KEYWORD(attr_severity, "Severity", LogLevel)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(attr_timestamp, "TimeStamp", DateTime)


enum AppenderFlags {
    APPENDER_FLAGS_NONE = 0x00,
    APPENDER_FLAGS_TIMESTAMP = 0x01,
    APPENDER_FLAGS_SEVERITY = 0x02,
    APPENDER_FLAGS_CHANNEL = 0x04,
    APPENDER_FLAGS_ALL = 0x07,
};

class Logger;

class TC_COMMON_API Appender: public boost::noncopyable {
public:
    typedef sinks::basic_formatting_sink_frontend<char> SinkType;
    typedef boost::shared_ptr<SinkType> SinkTypePtr;

    Appender(std::string name,
             LogLevel level,
             AppenderFlags flags)
            : _name(std::move(name))
            , _level(level)
            , _flags(flags) {

    }

    virtual ~Appender();

    const std::string& getName() const {
        return _name;
    }

    LogLevel getLogLevel() const {
        return _level;
    }

    AppenderFlags getFlags() const {
        return _flags;
    }

    SinkTypePtr makeSink() const {
        auto sink = createSink();
        setFilter(sink);
        setFormatter(sink);
        return sink;
    }

    void addLogger(const Logger *logger) {
        _loggers.insert(logger->getName());
    }

    void delLogger(const Logger *logger) {
        _loggers.insert(logger->getName());
    }
protected:
    virtual SinkTypePtr createSink() const = 0;

    void setFilter(SinkTypePtr sink) const {
        sink->set_filter(boost::phoenix::bind(&Appender::filter, this, attr_severity.or_none(),
                                              attr_channel.or_none()));
    }

    void setFormatter(SinkTypePtr sink) const {
        sink->set_formatter(boost::phoenix::bind(&Appender::formatter, this, boost::phoenix::arg_names::arg1,
                                                 boost::phoenix::arg_names::arg2));
    }

    bool filter(const logging::value_ref<LogLevel, tag::attr_severity> &level,
                const logging::value_ref<std::string, tag::attr_channel> &channel) const;
    void formatter(const logging::record_view &rec, logging::formatting_ostream &strm) const;

    std::string _name;
    LogLevel _level;
    AppenderFlags _flags;
    std::set<std::string> _loggers;
};


template<typename T>
class AppenderFactory {
public:
    Appender* operator()(std::string name, LogLevel level, AppenderFlags flags, const StringVector& extraArgs) {
        Appender *appender = _factory(name, level, flags, extraArgs);
        auto core = logging::core::get();
        core->add_sink(appender->makeSink());
        return appender;
    }
protected:
    boost::factory<T*> _factory;
};

#endif //TINYCORE_APPENDER_H
