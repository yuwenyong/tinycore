//
// Created by yuwenyong on 17-8-4.
//

#include "tinycore/logging/sinks.h"
#include <boost/core/null_deleter.hpp>


void BaseSink::onSetFilter(FrontendSinkPtr sink) {
    if (_filter) {
        sink->set_filter(logging::parse_filter(*_filter));
    } else if (_name) {
        sink->set_filter(boost::phoenix::bind(&BaseSink::filter, attr_severity.or_none(), attr_channel.or_none(),
                                              *_name, _level));
    } else {
        sink->set_filter(attr_severity >= _level);
    }
}

void BaseSink::onSetFormatter(FrontendSinkPtr sink) {
    if (_formatter) {
        sink->set_formatter(logging::parse_formatter(*_formatter));
    } else {
        sink->set_formatter(expr::stream << '[' << expr::format_date_time("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") <<
                                         "][" << expr::attr<LogLevel>("Severity") << ']' << expr::smessage);
    }
}

bool BaseSink::filter(const logging::value_ref <LogLevel, tag::attr_severity> &sev,
                      const logging::value_ref <std::string, tag::attr_channel> &channel,
                      const std::string &name, LogLevel level) {
    if (sev < level) {
        return false;
    }
    return ChannelFilterFactory::isChildOf(channel.get(), name);
}


ConsoleSink::FrontendSinkPtr ConsoleSink::createSink() {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

FrontendSinkPtr ConsoleSink::createAsyncSink() {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    return sink;
}

ConsoleSink::BackendSinkPtr ConsoleSink::createBackend() {
    auto backend = boost::make_shared<BackendSink>();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    backend->auto_flush(_autoFlush);
    return backend;
}


FileSink::BackendSinkPtr FileSink::createBackend() {
    auto backend = boost::make_shared<BackendSink>();
    backend->add_stream(boost::make_shared< std::ofstream >(_fileName));
    backend->auto_flush(_autoFlush);
    return backend;
}


FrontendSinkPtr RotatingFileSink::createSink() {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

FrontendSinkPtr RotatingFileSink::createAsyncSink() {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    return sink;
}

RotatingFileSink::BackendSinkPtr RotatingFileSink::createBackend() {
    auto backend = boost::make_shared<BackendSink>(
            keywords::file_name = _fileName,
            keywords::open_mode = _mode,
            keywords::rotation_size = _maxFileSize,
            keywords::auto_flush = _autoFlush;
    );
    return backend;
}


BackendSinkPtr TimedRotatingFileSink::createBackend() {
    auto backend = boost::make_shared<BackendSink>(
            keywords::file_name = _fileName,
            keywords::open_mode = _mode,
            keywords::rotation_size = _maxFileSize,
            keywords::auto_flush = _autoFlush;
    );
    return backend;
}