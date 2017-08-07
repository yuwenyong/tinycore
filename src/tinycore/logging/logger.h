//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_LOGGER_H
#define TINYCORE_LOGGER_H

#include "tinycore/common/common.h"
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/keywords/channel.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scope_exit.hpp>
#include "tinycore/logging/attributes.h"
#include "tinycore/utilities/string.h"


namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;


namespace logger_keywords {
BOOST_PARAMETER_KEYWORD(file_ns, file)
BOOST_PARAMETER_KEYWORD(line_ns, line)
BOOST_PARAMETER_KEYWORD(func_ns, func)
}


template <typename BaseT>
class PositionTaggerFeature: public BaseT {
public:
    typedef typename BaseT::char_type char_type;
    typedef typename BaseT::threading_model threading_model;

    typedef typename logging::strictest_lock<
            boost::lock_guard<threading_model>,
            typename BaseT::open_record_lock,
            typename BaseT::add_attribute_lock,
            typename BaseT::remove_attribute_lock
    >::type open_record_lock;

    PositionTaggerFeature() = default;

    PositionTaggerFeature(PositionTaggerFeature const& that)
            : BaseT(static_cast<BaseT const&>(that)) {

    }

    template< typename ArgsT >
    PositionTaggerFeature(ArgsT const& args)
            : BaseT(args) {

    }

protected:
    template< typename ArgsT >
    logging::record open_record_unlocked(ArgsT const& args) {
        logging::string_literal fileValue = args[logger_keywords::file | logging::string_literal()];
        size_t lineValue = args[logger_keywords::line | 0];
        logging::string_literal funcValue = args[logger_keywords::func | logging::string_literal()];

        logging::attribute_set &attrs = BaseT::attributes();
        logging::attribute_set::iterator fileIter = attrs.end();
        logging::attribute_set::iterator lineIter = attrs.end();
        logging::attribute_set::iterator funcIter = attrs.end();

        if (!fileValue.empty()) {
            auto res = BaseT::add_attribute_unlocked("File", attrs::constant<logging::string_literal>(fileValue));
            if (res.second) {
                fileIter = res.first;
            }
        }
        if (lineValue != 0) {
            auto res = BaseT::add_attribute_unlocked("Line", attrs::constant<size_t>(lineValue));
            if (res.second) {
                lineIter = res.first;
            }
        }
        if (!funcValue.empty()) {
            auto res = BaseT::add_attribute_unlocked("Func", attrs::constant<size_t>(funcValue));
            if (res.second) {
                funcIter = res.first;
            }
        }

        BOOST_SCOPE_EXIT_TPL(&fileIter, &lineIter, &funcIter, &attrs) {
            if (fileIter != attrs.end()) {
                attrs.erase(fileIter);
            }
            if (lineIter != attrs.end()) {
                attrs.erase(lineIter);
            }
            if (funcIter != attrs.end()) {
                attrs.erase(funcIter);
            }
        }BOOST_SCOPE_EXIT_END

        return BaseT::open_record_unlocked(args);
    }
};


struct PositionTagger: public boost::mpl::quote1<PositionTaggerFeature> {

};


class LoggerImpl: public boost::noncopyable {
public:
    typedef src::severity_channel_logger_mt<LogLevel, std::string> LoggerType;
    typedef logging::record LogRecord;

    LoggerImpl(std::string name, LogLevel level=LOG_LEVEL_DISABLED)
            : _name(std::move(name))
            , _logger(keywords::channel=_name)
            , _level(level) {

    }

    ~LoggerImpl() {

    }

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
        write(LOG_LEVEL_DEBUG, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(const char *format, Args&&... args) {
        write(LOG_LEVEL_INFO, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(const char *format, Args&&... args) {
        write(LOG_LEVEL_WARN, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const char *format, Args&&... args) {
        write(LOG_LEVEL_ERROR, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(const char *format, Args&&... args) {
        write(LOG_LEVEL_FATAL, format, std::forward<Args>(args)...);
    }
protected:
    bool shouldLog(LogLevel level) const {
        return _level != LOG_LEVEL_DISABLED && _level <= level;
    }

    template <typename... Args>
    void write(LogLevel level, const char *format, Args&&... args) {
        if (!shouldLog(level)) {
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
