//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_LOGGER_H
#define TINYCORE_LOGGER_H

#include "tinycore/common/common.h"
#include <boost/functional/factory.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>
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
        StringLiteral fileValue = args[logger_keywords::file | StringLiteral()];
        size_t lineValue = args[logger_keywords::line | 0];
        StringLiteral funcValue = args[logger_keywords::func | StringLiteral()];

        logging::attribute_set &attrs = BaseT::attributes();
        logging::attribute_set::iterator fileIter = attrs.end();
        logging::attribute_set::iterator lineIter = attrs.end();
        logging::attribute_set::iterator funcIter = attrs.end();

        if (!fileValue.empty()) {
            auto res = BaseT::add_attribute_unlocked("File", attrs::constant<StringLiteral>(fileValue));
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
            auto res = BaseT::add_attribute_unlocked("Func", attrs::constant<StringLiteral>(funcValue));
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


template <typename LevelT = int, typename ChannelT = std::string>
class PositionLoggerMT: public src::basic_composite_logger<
        char, PositionLoggerMT<LevelT, ChannelT>,
        src::multi_thread_model<logging::aux::light_rw_mutex>,
        src::features<src::severity<LevelT>, src::channel<ChannelT>, PositionTagger>> {
    BOOST_LOG_FORWARD_LOGGER_MEMBERS_TEMPLATE(PositionLoggerMT)
};


class Logger: public boost::noncopyable {
public:
    friend class boost::factory<Logger*>;

    const std::string& getName() const {
        return _name;
    }

    Logger* getChild(const std::string &suffix) const;

    template <typename... Args>
    void trace(const char *format, Args&&... args) {
        write(LOG_LEVEL_TRACE, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args) {
        write(file, line, func, LOG_LEVEL_TRACE, format, std::forward<Args>(args)...);
    }

    void trace(const Byte *data, size_t length) {
        write(LOG_LEVEL_TRACE, data, length);
    }

    void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_TRACE, data, length);
    }

    template <typename... Args>
    void debug(const char *format, Args&&... args) {
        write(LOG_LEVEL_DEBUG, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args) {
        write(file, line, func, LOG_LEVEL_DEBUG, format, std::forward<Args>(args)...);
    }

    void debug(const Byte *data, size_t length) {
        write(LOG_LEVEL_DEBUG, data, length);
    }

    void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_DEBUG, data, length);
    }

    template <typename... Args>
    void info(const char *format, Args&&... args) {
        write(LOG_LEVEL_INFO, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args) {
        write(file, line, func, LOG_LEVEL_INFO, format, std::forward<Args>(args)...);
    }

    void info(const Byte *data, size_t length) {
        write(LOG_LEVEL_INFO, data, length);
    }

    void info(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_INFO, data, length);
    }

    template <typename... Args>
    void warning(const char *format, Args&&... args) {
        write(LOG_LEVEL_WARNING, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args){
        write(file, line, func, LOG_LEVEL_WARNING, format, std::forward<Args>(args)...);
    }

    void warning(const Byte *data, size_t length) {
        write(LOG_LEVEL_WARNING, data, length);
    }

    void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_WARNING, data, length);
    }

    template <typename... Args>
    void error(const char *format, Args&&... args) {
        write(LOG_LEVEL_ERROR, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args) {
        write(file, line, func, LOG_LEVEL_ERROR, format, std::forward<Args>(args)...);
    }

    void error(const Byte *data, size_t length) {
        write(LOG_LEVEL_ERROR, data, length);
    }

    void error(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_ERROR, data, length);
    }

    template <typename... Args>
    void fatal(const char *format, Args&&... args) {
        write(LOG_LEVEL_FATAL, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format, Args&&... args) {
        write(file, line, func, LOG_LEVEL_FATAL, format, std::forward<Args>(args)...);
    }

    void fatal(const Byte *data, size_t length) {
        write(LOG_LEVEL_FATAL, data, length);
    }

    void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data, size_t length) {
        write(file, line, func, LOG_LEVEL_FATAL, data, length);
    }
protected:
    explicit Logger(std::string name)
            : _name(std::move(name))
            , _logger(keywords::channel=_name) {

    }

    template <typename... Args>
    void write(LogLevel level, const char *format, Args&&... args) {
        logging::record rec = _logger.open_record(keywords::severity=level);
        if (rec) {
            logging::record_ostream strm(rec);
            strm << String::format(format, std::forward<Args>(args)...);
            _logger.push_record(std::move(rec));
        }
    }

    template <typename... Args>
    void write(const StringLiteral &file, size_t line, const StringLiteral &func, LogLevel level, const char *format,
               Args&&... args) {
        logging::record rec = _logger.open_record((keywords::severity=level, logger_keywords::file=file,
                logger_keywords::line=line, logger_keywords::func=func));
        if (rec) {
            logging::record_ostream strm(rec);
            strm << String::format(format, std::forward<Args>(args)...);
            _logger.push_record(std::move(rec));
        }
    }

    void write(LogLevel level, const Byte *data, size_t length);
    void write(const StringLiteral &file, size_t line, const StringLiteral &func, LogLevel level,
               const Byte *data, size_t length);

    std::string _name;
    PositionLoggerMT<LogLevel> _logger;
};


#endif //TINYCORE_LOGGER_H
