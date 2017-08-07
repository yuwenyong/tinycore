//
// Created by yuwenyong on 17-8-4.
//

#ifndef TINYCORE_SINKS_H
#define TINYCORE_SINKS_H

#include "tinycore/common/common.h"
#include <boost/log/sinks.hpp>
#include <boost/log/utility/functional.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include "tinycore/logging/attributes.h"


namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;


class TC_COMMON_API BaseSink {
public:
    typedef sinks::basic_formatting_sink_frontend<char> FrontendSink;
    typedef boost::shared_ptr<FrontendSink> FrontendSinkPtr;

    void setFilter(LogLevel level) {
        _level = level;
    }

    void setFilter(std::string name, LogLevel level) {
        _name = std::move(name);
        setFilter(level);
    }

    void setFilter(std::string filter) {
        _filter = std::move(filter);
    }

    void setFormatter(std::string formatter) {
        _formatter = std::move(formatter);
    }

    void setAsync(bool async) {
        _async = async;
    }

    FrontendSinkPtr makeSink() {
        FrontendSinkPtr sink;
        if (_async) {
            sink = createSink();
        } else {
            sink = createAsyncSink();
        }
        onSetFilter(sink);
        onSetFormatter(sink);
        return sink;
    }

    virtual ~BaseSink() = default;
protected:
    virtual FrontendSinkPtr createSink() = 0;
    virtual FrontendSinkPtr createAsyncSink() = 0;
    void onSetFilter(FrontendSinkPtr sink);
    void onSetFormatter(FrontendSinkPtr sink);

    static bool filter(const logging::value_ref<LogLevel, tag::attr_severity> &sev,
                       const logging::value_ref<std::string, tag::attr_channel> &channel,
                       const std::string &name, LogLevel level);

    LogLevel _level{LOG_LEVEL_INFO};
    boost::optional<std::string> _name;
    boost::optional<std::string> _formatter;
    boost::optional<std::string> _filter;
    bool _async{false};
};


class TC_COMMON_API ConsoleSink: public BaseSink {
public:
    typedef sinks::text_ostream_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    explicit ConsoleSink(bool autoFlush=true)
            : _autoFlush(autoFlush) {

    }

    void setAutoFlush(bool autoFlush) {
        _autoFlush = autoFlush;
    }

protected:
    FrontendSinkPtr createSink() override;
    FrontendSinkPtr createAsyncSink() override;
    virtual BackendSinkPtr createBackend();

    bool _autoFlush;
};


class TC_COMMON_API FileSink: public ConsoleSink {
public:
    explicit FileSink(std::string fileName, bool autoFlush=true)
            : ConsoleSink(autoFlush)
            , _fileName(std::move(fileName)) {

    }
protected:
    BackendSinkPtr createBackend() override;

    std::string _fileName;
};


class TC_COMMON_API RotatingFileSink: public BaseSink {
public:
    typedef sinks::text_file_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    explicit RotatingFileSink(std::string fileName,
                              std::ios_base::openmode mode = std::ios_base::app|std::ios_base::out,
                              size_t maxFileSize = 5 * 1024 * 1024, bool autoFlush = true)
            : _fileName(std::move(fileName))
            , _mode(mode)
            , _maxFileSize(maxFileSize)
            , _autoFlush(autoFlush) {

    }

    void setOpenMode(std::ios_base::openmode mode) {
        _mode = mode;
    }

    void setMaxFileSize(size_t maxFileSize) {
        _maxFileSize = maxFileSize;
    }

    void setAutoFlush(bool autoFlush) {
        _autoFlush = autoFlush;
    }
protected:
    FrontendSinkPtr createSink() override;
    FrontendSinkPtr createAsyncSink() override;
    virtual BackendSinkPtr createBackend();

    std::string _fileName;
    std::ios_base::openmode _mode;
    size_t _maxFileSize;
    bool _autoFlush;
};


class TC_COMMON_API TimedRotatingFileSink: public RotatingFileSink {
public:
    using TimePoint = sinks::file::rotation_at_time_point;
    using TimeInterval = sinks::file::rotation_at_time_interval;

    class RotationTimeVisitor: public boost::static_visitor<BackendSinkPtr> {
    public:
        RotationTimeVisitor(std::string fileName, std::ios_base::openmode mode, size_t maxFileSize,
                            bool autoFlush)
                : _fileName(std::move(fileName))
                , _mode(mode)
                , _maxFileSize(maxFileSize)
                , _autoFlush(autoFlush) {

        }

        BackendSinkPtr operator()(const TimePoint &timePoint) const {
            auto backend = boost::make_shared<BackendSink>(
                    keywords::file_name = _fileName,
                    keywords::open_mode = _mode,
                    keywords::rotation_size = _maxFileSize,
                    keywords::auto_flush = _autoFlush,
                    keywords::time_based_rotation = timePoint
            );
            return backend;
        }

        BackendSinkPtr operator()(const TimeInterval &timeInterval) const {
            auto backend = boost::make_shared<BackendSink>(
                    keywords::file_name = _fileName,
                    keywords::open_mode = _mode,
                    keywords::rotation_size = _maxFileSize,
                    keywords::auto_flush = _autoFlush,
                    keywords::time_based_rotation = timeInterval
            );
            return backend;
        }

    protected:
        std::string _fileName;
        std::ios_base::openmode _mode;
        size_t _maxFileSize;
        bool _autoFlush;
    };

    explicit TimedRotatingFileSink(std::string fileName,
                                   const TimePoint &timePoint = sinks::file::rotation_at_time_point(0, 0, 0),
                                   std::ios_base::openmode mode = std::ios_base::app | std::ios_base::out,
                                   size_t maxFileSize = 5 * 1024 * 1024, bool autoFlush = true)
            : RotatingFileSink(std::move(fileName), mode, maxFileSize, autoFlush)
            , _rotationTime(timePoint) {

    }

    TimedRotatingFileSink(std::string fileName, const TimeInterval &timeInterval,
                          std::ios_base::openmode mode = std::ios_base::app | std::ios_base::out,
                          size_t maxFileSize = 5 * 1024 * 1024, bool autoFlush = true)
            : RotatingFileSink(std::move(fileName), mode, maxFileSize, autoFlush)
            , _rotationTime(timeInterval) {

    }

    void setRotationTimePoint(const TimePoint &timePoint) {
        _rotationTime = timePoint;
    }

    void setRotationTimeInterval(const TimeInterval &timeInterval) {
        _rotationTime = timeInterval;
    }
protected:
    BackendSinkPtr createBackend() override;

    boost::variant<TimePoint, TimeInterval> _rotationTime;
};

#ifndef BOOST_LOG_WITHOUT_SYSLOG

using SyslogFacility = sinks::syslog::facility;

#define SYSLOG_FACILITY_KERNEL      sinks::syslog::kernel
#define SYSLOG_FACILITY_USER        sinks::syslog::user
#define SYSLOG_FACILITY_MAIL        sinks::syslog::mail
#define SYSLOG_FACILITY_DAEMON      sinks::syslog::daemon
#define SYSLOG_FACILITY_SECURITY0   sinks::syslog::security0
#define SYSLOG_FACILITY_SYSLOGD     sinks::syslog::syslogd
#define SYSLOG_FACILITY_PRINTER     sinks::syslog::printer
#define SYSLOG_FACILITY_NEWS        sinks::syslog::news
#define SYSLOG_FACILITY_UUCP        sinks::syslog::uucp
#define SYSLOG_FACILITY_CLOCK0      sinks::syslog::clock0
#define SYSLOG_FACILITY_SECURITY1   sinks::syslog::security1
#define SYSLOG_FACILITY_FTP         sinks::syslog::ftp
#define SYSLOG_FACILITY_NTP         sinks::syslog::ntp
#define SYSLOG_FACILITY_LOG_AUDIT   sinks::syslog::log_audit
#define SYSLOG_FACILITY_LOG_ALERT   sinks::syslog::log_alert
#define SYSLOG_FACILITY_CLOCK1      sinks::syslog::clock1
#define SYSLOG_FACILITY_LOCAL0      sinks::syslog::local0
#define SYSLOG_FACILITY_LOCAL1      sinks::syslog::local1
#define SYSLOG_FACILITY_LOCAL2      sinks::syslog::local2
#define SYSLOG_FACILITY_LOCAL3      sinks::syslog::local3
#define SYSLOG_FACILITY_LOCAL4      sinks::syslog::local4
#define SYSLOG_FACILITY_LOCAL5      sinks::syslog::local5
#define SYSLOG_FACILITY_LOCAL6      sinks::syslog::local6
#define SYSLOG_FACILITY_LOCAL7      sinks::syslog::local7


class TC_COMMON_API SyslogSink: public BaseSink {
public:
    typedef sinks::syslog_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    explicit SyslogSink(SyslogFacility facility=SYSLOG_FACILITY_USER)
            : _facility(facility) {

    }

    explicit SyslogSink(std::string targetAddress, unsigned short targetPort=514,
                        SyslogFacility facility=SYSLOG_FACILITY_USER)
            : _targetAddress(std::move(targetAddress))
            , _targetPort(targetPort)
            , _facility(facility) {

    }
protected:
    FrontendSinkPtr createSink() override;
    FrontendSinkPtr createAsyncSink() override;
    BackendSinkPtr createBackend();

    boost::optional<std::string> _targetAddress;
    unsigned short _targetPort{514};
    SyslogFacility _facility{SYSLOG_FACILITY_USER};
};

#endif


#ifndef BOOST_LOG_WITHOUT_EVENT_LOG

using RegistrationMode = sinks::event_log::registration_mode;

#define REGISTRATION_MODE_NEVER     sinks::event_log::never
#define REGISTRATION_MODE_ON_DEMAND sinks::event_log::on_demand
#define REGISTRATION_MODE_FORCED    sinks::event_log::forced


class TC_COMMON_API SimpleEventLogSink: public BaseSink {
public:
    typedef sinks::simple_event_log_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    explicit SimpleEventLogSink(std::string logName="", std::string logSource="",
                                RegistrationMode registrationMode=REGISTRATION_MODE_ON_DEMAND)
            : _logName(std::move(logName))
            , _logSource(std::move(logSource)
            , _registrationMode(registrationMode) {

    }

    void setLogName(std::string logName) {
        _logName = std::move(logName);
    }

    void setLogSource(std::string logSource) {
        _logSource = std::move(logSource);
    }

    void setRegistrationMode(RegistrationMode registrationMode) {
        _registrationMode = registrationMode;
    }
protected:
    FrontendSinkPtr createSink() override;
    FrontendSinkPtr createAsyncSink() override;
    BackendSinkPtr createBackend();

    std::string _logName;
    std::string _logSource;
    RegistrationMode _registrationMode{REGISTRATION_MODE_ON_DEMAND};
};

#endif


#ifndef BOOST_LOG_WITHOUT_DEBUG_OUTPUT

class TC_COMMON_API DebuggerSink: public BaseSink {
public:
    typedef sinks::debug_output_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    DebuggerSink() = default;
protected:
    FrontendSinkPtr createSink() override;
    FrontendSinkPtr createAsyncSink() override;
    BackendSinkPtr createBackend();
};

#endif


class BasicSinkFactory: public logging::sink_factory<char> {
protected:
    typedef logging::sink_factory<char> base_type;
    typedef typename base_type::char_type char_type;
    typedef typename base_type::string_type string_type;
    typedef typename base_type::settings_section settings_section;

    static bool paramCastToBool(const string_type &param);

    template <typename BackendT>
    static boost::shared_ptr<sinks::sink> initSink(boost::shared_ptr<BackendT> const& backend,
                                                   settings_section const& params) {
        typedef BackendT backend_t;
        logging::filter filt;
        if (boost::optional<string_type> filterParam = params["Filter"]) {
            filt = logging::parse_filter(filterParam.get());
        }
        boost::shared_ptr<sinks::basic_sink_frontend> p;
        bool async = false;
        if (boost::optional<string_type> asyncParam = params["Asynchronous"]) {
            async = paramCastToBool(asyncParam.get());
        }
        if (!async) {
            p = initFormatter(boost::make_shared<sinks::synchronous_sink<backend_t>>(backend), params);
        } else {
            p = initFormatter(boost::make_shared<sinks::asynchronous_sink<backend_t>>(backend), params);
            p->set_exception_handler(logging::nop());
        }
        p->set_filter(filt);
        return p;
    }

    template <typename SinkT>
    static boost::shared_ptr<SinkT> initFormatter(boost::shared_ptr<SinkT> const& sink,
                                                  settings_section const& params) {
        if (boost::optional<string_type> formatParam = params["Format"]) {
            sink->set_formatter(logging::parse_formatter(formatParam.get()));
        }
        return sink;
    }
};


class FileSinkFactory: public BasicSinkFactory {
public:
    typedef sinks::text_ostream_backend BackendSink;
    typedef boost::shared_ptr<BackendSink> BackendSinkPtr;

    boost::shared_ptr<sinks::sink> create_sink(settings_section const& settings) override;
};


#endif //TINYCORE_SINKS_H
