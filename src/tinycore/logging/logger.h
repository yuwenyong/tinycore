//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_LOGGER_H
#define TINYCORE_LOGGER_H

#include "tinycore/common/common.h"
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/keywords/channel.hpp>
#include "tinycore/utilities/util.h"

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;

enum LogLevel {
    LOG_LEVEL_DISABLED,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};


template< typename CharT, typename TraitsT >
std::basic_ostream< CharT, TraitsT >& operator<< (std::basic_ostream< CharT, TraitsT>& strm, LogLevel lvl) {
    static const char* const str[] = {
            "DISABLED",
            "DEBUG",
            "INFO ",
            "WARN ",
            "ERROR",
            "FATAL",
    };
    if (static_cast<size_t>(lvl) < (sizeof(str) / sizeof(*str))) {
        strm << str[lvl];
    } else {
        strm << static_cast<int>(lvl);
    }
    return strm;
}


class Logger: public NonCopyable {
public:
    typedef src::severity_channel_logger_mt<LogLevel, std::string> LoggerType;
    typedef logging::record LogRecord;

    Logger(std::string name, LogLevel level=LOG_LEVEL_DISABLED)
            : _name(std::move(name))
            , _logger(keywords::channel=_name)
            , _level(level) {

    }

    ~Logger();

    const std::string& getName() const {
        return _name;
    }

    LogLevel getLoggerLevel() const {
        return _level;
    }

    void setLoggerLevel(LogLevel level) {
        _level = level;
    }

    template <typename... Args>
    void debug(const char *format, Args&&... args) {
        _write(LOG_LEVEL_DEBUG, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(const char *format, Args&&... args) {
        _write(LOG_LEVEL_INFO, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(const char *format, Args&&... args) {
        _write(LOG_LEVEL_WARN, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const char *format, Args&&... args) {
        _write(LOG_LEVEL_ERROR, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(const char *format, Args&&... args) {
        _write(LOG_LEVEL_FATAL, format, std::forward<Args>(args)...);
    }
protected:
    bool _shouldLog(LogLevel level) const {
        return _level != LOG_LEVEL_DISABLED && _level <= level;
    }

    template <typename... Args>
    void _write(LogLevel level, const char *format, Args&&... args) {
        if (!_shouldLog(level)) {
            return;
        }
        LogRecord rec = _logger.open_record(keywords::severity=level);
        if (rec) {
            boost::log::record_ostream strm(rec);
            strm << String::format(format, std::forward<Args>(args)...);
            _logger.push_record(std::move(rec));
        }
    }

    std::string _name;
    LoggerType _logger;
    LogLevel _level{LOG_LEVEL_DISABLED};
};

#endif //TINYCORE_LOGGER_H
