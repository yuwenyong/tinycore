//
// Created by yuwenyong on 17-8-4.
//

#ifndef TINYCORE_SINKS_H
#define TINYCORE_SINKS_H

#include "tinycore/common/common.h"
#include <boost/log/sinks.hpp>
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

    LogLevel _level{LOG_LEVEL_UNSET};
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

    TimedRotatingFileSink(std::string fileName, const TimePoint &timePoint,
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


#endif //TINYCORE_SINKS_H
