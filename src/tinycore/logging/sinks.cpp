//
// Created by yuwenyong on 17-8-4.
//

#include "tinycore/logging/sinks.h"
#include <boost/algorithm/string.hpp>
#include <boost/core/null_deleter.hpp>
#include "tinycore/logging/logging.h"


void BaseSink::setFilter(const std::string &name, LogLevel level) {
    _name = Logging::getLoggerName(name);
    setFilter(level);
}


void BaseSink::onSetFilter(FrontendSinkPtr sink) const {
    if (_filter) {
        sink->set_filter(logging::parse_filter(*_filter));
    } else if (_name) {
        sink->set_filter(boost::phoenix::bind(&BaseSink::filter, attr_severity.or_none(), attr_channel.or_none(),
                                              *_name, _level));
    } else {
        sink->set_filter(attr_severity >= _level);
    }
}

void BaseSink::onSetFormatter(FrontendSinkPtr sink) const {
    if (_formatter) {
        sink->set_formatter(logging::parse_formatter(*_formatter));
    } else {
        sink->set_formatter(expr::stream << '[' << expr::format_date_time<DateTime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                                         << "][" << expr::attr<LogLevel>("Severity") << ']' << expr::smessage);
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


ConsoleSink::FrontendSinkPtr ConsoleSink::createSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

ConsoleSink::FrontendSinkPtr ConsoleSink::createAsyncSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    sink->set_exception_handler(logging::nop());
    return sink;
}

ConsoleSink::BackendSinkPtr ConsoleSink::createBackend() const {
    auto backend = boost::make_shared<BackendSink>();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    backend->auto_flush(_autoFlush);
    return backend;
}


FileSink::BackendSinkPtr FileSink::createBackend() const {
    auto backend = boost::make_shared<BackendSink>();
    backend->add_stream(boost::make_shared<std::ofstream>(_fileName));
    backend->auto_flush(_autoFlush);
    return backend;
}


RotatingFileSink::FrontendSinkPtr RotatingFileSink::createSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

RotatingFileSink::FrontendSinkPtr RotatingFileSink::createAsyncSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    sink->set_exception_handler(logging::nop());
    return sink;
}

RotatingFileSink::BackendSinkPtr RotatingFileSink::createBackend() const {
    auto backend = boost::make_shared<BackendSink>(
            keywords::file_name = _fileName,
            keywords::open_mode = _mode,
            keywords::rotation_size = _maxFileSize,
            keywords::auto_flush = _autoFlush
    );
    return backend;
}


TimedRotatingFileSink::BackendSinkPtr TimedRotatingFileSink::createBackend() const {
    return boost::apply_visitor(RotationTimeVisitor(_fileName, _maxFileSize, _mode, _autoFlush), _rotationTime);
}

#ifndef BOOST_LOG_WITHOUT_SYSLOG

SyslogSink::FrontendSinkPtr SyslogSink::createSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

SyslogSink::FrontendSinkPtr SyslogSink::createAsyncSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    sink->set_exception_handler(logging::nop());
    return sink;
}

SyslogSink::BackendSinkPtr SyslogSink::createBackend() const {
    auto backend = boost::make_shared<BackendSink>(keywords::facility=_facility);
    if (_targetAddress) {
        backend->set_target_address(*_targetAddress, _targetPort);
    }
    sinks::syslog::custom_severity_mapping< std::string > mapping("Severity");
    mapping["trace"] = sinks::syslog::debug;
    mapping["debug"] = sinks::syslog::debug;
    mapping["info"] = sinks::syslog::info;
    mapping["warning"] = sinks::syslog::warning;
    mapping["error"] = sinks::syslog::error;
    mapping["fatal"] = sinks::syslog::critical;
    backend->set_severity_mapper(mapping);
    return backend;
}

#endif


#ifndef BOOST_LOG_WITHOUT_EVENT_LOG

SimpleEventLogSink::FrontendSinkPtr SimpleEventLogSink::createSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

SimpleEventLogSink::FrontendSinkPtr SimpleEventLogSink::createAsyncSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    sink->set_exception_handler(logging::nop());
    return sink;
}

SimpleEventLogSink::BackendSinkPtr SimpleEventLogSink::createBackend() const {
    std::string logName = _logName, logSource = _logSource;
    if (logName.empty()) {
        logName = BackendSink::get_default_log_name();
    }
    if (logSource.empty()) {
        logSource = BackendSink::get_default_source_name();
    }
    auto backend = boost::make_shared<BackendSink>(
            keywords::log_name = logName,
            keywords::log_source = logSource,
            keywords::registration = _registrationMode
    );
    sinks::event_log::custom_event_type_mapping<LogLevel> mapping("Severity");
    mapping[LOG_LEVEL_TRACE] = sinks::event_log::info;
    mapping[LOG_LEVEL_DEBUG] = sinks::event_log::info;
    mapping[LOG_LEVEL_INFO] = sinks::event_log::info;
    mapping[LOG_LEVEL_WARNING] = sinks::event_log::warning;
    mapping[LOG_LEVEL_ERROR] = sinks::event_log::error;
    mapping[LOG_LEVEL_FATAL] = sinks::event_log::error;
    backend->set_event_type_mapper(mapping);
    return backend;
}

#endif


#ifndef BOOST_LOG_WITHOUT_DEBUG_OUTPUT

DebuggerSink::FrontendSinkPtr DebuggerSink::createSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::synchronous_sink<BackendSink>>(backend);
    return sink;
}

DebuggerSink::FrontendSinkPtr DebuggerSink::createAsyncSink() const {
    auto backend = createBackend();
    auto sink = boost::make_shared<sinks::asynchronous_sink<BackendSink>>(backend);
    sink->set_exception_handler(logging::nop());
    return sink;
}

DebuggerSink::BackendSinkPtr DebuggerSink::createBackend() const {
    auto backend = boost::make_shared<BackendSink>();
    return backend;
}

#endif


bool BasicSinkFactory::paramCastToBool(const string_type &param) {
    if (boost::iequals(param, "true")) {
        return true;
    }
    if (boost::iequals(param, "false")) {
        return false;
    }
    return std::stoi(param) != 0;
}


boost::shared_ptr<sinks::sink> FileSinkFactory::create_sink(settings_section const &settings) {
    std::string fileName;
    if (boost::optional<std::string> param = settings["FileName"]) {
        fileName = param.get();
    } else {
        throw std::runtime_error("No target file name specified in settings");
    }
    bool autoFlush = false;
    if (boost::optional<string_type> autoFlushParam = settings["AutoFlush"]) {
        autoFlush = paramCastToBool(autoFlushParam.get());
    }
    auto backend = boost::make_shared<BackendSink>();
    backend->add_stream(boost::make_shared<std::ofstream>(fileName));
    backend->auto_flush(autoFlush);
    return initSink(backend, settings);
}