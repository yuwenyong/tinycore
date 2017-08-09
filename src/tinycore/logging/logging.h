//
// Created by yuwenyong on 17-8-8.
//

#ifndef TINYCORE_LOGGING_H
#define TINYCORE_LOGGING_H

#include "tinycore/common/common.h"
#include <mutex>
#include <boost/log/core.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include "tinycore/logging/logger.h"
#include "tinycore/logging/sinks.h"


class TC_COMMON_API Logging {
public:
    friend class Logger;
    typedef logging::settings Settings;
    typedef boost::ptr_map<std::string, Logger> LoggerMap;

    static void init();

    static void initFromSettings(const Settings &settings);

    static void initFromFile(const std::string &fileName);

    static void close() {
        logging::core::get()->flush();
        logging::core::get()->remove_all_sinks();
    }

    static void disable() {
        logging::core::get()->set_logging_enabled(false);
    }

    static void enable() {
        logging::core::get()->set_logging_enabled(true);
    }

    static void setFilter(LogLevel level) {
        logging::core::get()->set_filter(attr_severity >= level);
    }

    static void setFilter(const std::string &filter) {
        logging::core::get()->set_filter(logging::parse_filter(filter));
    }

    static void resetFilter() {
        logging::core::get()->reset_filter();
    }

    static Logger* getRootLogger() {
        return _rootLogger;
    }

    static Logger* getLogger(const std::string &name="") {
        std::string loggerName = getLoggerName(name);
        return getLoggerByName(loggerName);
    }

    static std::string getLoggerName(const std::string &name) {
        std::string prefix("root");
        if (name.empty()) {
            return prefix;
        } else {
            return prefix + '.' + name;
        }
    }

    static void addSink(const BaseSink &sink) {
        logging::core::get()->add_sink(sink.makeSink());
    }

    template <typename... Args>
    static void trace(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->trace(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->trace(file, line, func, format, std::forward<Args>(args)...);
    }

    static void trace(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->trace(data, length);
    }

    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->trace(file, line, func, data, length);
    }

    template <typename... Args>
    static void debug(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->debug(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->debug(file, line, func, format, std::forward<Args>(args)...);
    }

    static void debug(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->debug(data, length);
    }

    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->debug(file, line, func, data, length);
    }

    template <typename... Args>
    static void info(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->info(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                     Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->info(file, line, func, format, std::forward<Args>(args)...);
    }

    static void info(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->info(data, length);
    }

    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                     size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->info(file, line, func, data, length);
    }

    template <typename... Args>
    static void warning(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->warning(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                        Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->warning(file, line, func, format, std::forward<Args>(args)...);
    }

    static void warning(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->warning(data, length);
    }

    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                        size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->warning(file, line, func, data, length);
    }

    template <typename... Args>
    static void error(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->error(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->error(file, line, func, format, std::forward<Args>(args)...);
    }

    static void error(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->error(data, length);
    }

    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->error(file, line, func, data, length);
    }

    template <typename... Args>
    static void fatal(const char *format, Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->fatal(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        assert(_rootLogger != nullptr);
        _rootLogger->fatal(file, line, func, format, std::forward<Args>(args)...);
    }

    static void fatal(const Byte *data, size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->fatal(data, length);
    }

    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        assert(_rootLogger != nullptr);
        _rootLogger->fatal(file, line, func, data, length);
    }
protected:
    static void onPreInit();

    static void onPostInit();

    static Logger* getLoggerByName(std::string &loggerName);

    static Logger* getChildLogger(const Logger *logger, const std::string &suffix) {
        std::string loggerName = joinLoggerName(logger->getName(), suffix);
        return getLoggerByName(loggerName);
    }

    static std::string joinLoggerName(const std::string &name, const std::string &suffix) {
        assert(!name.empty() && !suffix.empty());
        return name + '.' + suffix;
    }

    static std::mutex _lock;
    static LoggerMap _loggers;
    static Logger *_rootLogger;
};


class TC_COMMON_API LoggingHelper {
public:
    template <typename... Args>
    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        Logging::trace(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const char *format, Args&&... args) {
        logger->trace(file, line, func, format, std::forward<Args>(args)...);
    }


    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        Logging::trace(file, line, func, data, length);
    }

    static void trace(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const Byte *data, size_t length) {
        logger->trace(file, line, func, data, length);
    }

    template <typename... Args>
    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        Logging::debug(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const char *format, Args&&... args) {
        logger->debug(file, line, func, format, std::forward<Args>(args)...);
    }


    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        Logging::debug(file, line, func, data, length);
    }

    static void debug(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const Byte *data, size_t length) {
        logger->debug(file, line, func, data, length);
    }

    template <typename... Args>
    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                     Args&&... args) {
        Logging::info(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                     const char *format, Args&&... args) {
        logger->info(file, line, func, format, std::forward<Args>(args)...);
    }


    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                     size_t length) {
        Logging::info(file, line, func, data, length);
    }

    static void info(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                     const Byte *data, size_t length) {
        logger->info(file, line, func, data, length);
    }

    template <typename... Args>
    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                        Args&&... args) {
        Logging::warning(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                        const char *format, Args&&... args) {
        logger->warning(file, line, func, format, std::forward<Args>(args)...);
    }


    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                        size_t length) {
        Logging::warning(file, line, func, data, length);
    }

    static void warning(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                        const Byte *data, size_t length) {
        logger->warning(file, line, func, data, length);
    }

    template <typename... Args>
    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        Logging::error(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const char *format, Args&&... args) {
        logger->error(file, line, func, format, std::forward<Args>(args)...);
    }


    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        Logging::error(file, line, func, data, length);
    }

    static void error(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const Byte *data, size_t length) {
        logger->error(file, line, func, data, length);
    }

    template <typename... Args>
    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const char *format,
                      Args&&... args) {
        Logging::fatal(file, line, func, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const char *format, Args&&... args) {
        logger->fatal(file, line, func, format, std::forward<Args>(args)...);
    }


    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, const Byte *data,
                      size_t length) {
        Logging::fatal(file, line, func, data, length);
    }

    static void fatal(const StringLiteral &file, size_t line, const StringLiteral &func, Logger *logger,
                      const Byte *data, size_t length) {
        logger->fatal(file, line, func, data, length);
    }
};


#define LOG_TRACE(...)      LoggingHelper::trace(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#ifdef NDEBUG
#define LOG_DEBUG(...)
#else
#define LOG_DEBUG(...)      LoggingHelper::debug(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif
#define LOG_INFO(...)       LoggingHelper::info(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARNING(...)    LoggingHelper::warning(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(...)      LoggingHelper::error(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_FATAL(...)      LoggingHelper::fatal(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)


#endif //TINYCORE_LOGGING_H
