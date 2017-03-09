//
// Created by yuwenyong on 17-2-4.
//

#ifndef TINYCORE_LOG_H
#define TINYCORE_LOG_H

#include "tinycore/common/common.h"
#include <functional>
#include <boost/log/core.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/mpl/string.hpp>
#include "tinycore/logging/appender.h"
#include "tinycore/logging/appendercustom.h"
#include "tinycore/logging/logger.h"
#include "tinycore/debugging/trace.h"


namespace logging = boost::log;


class TC_COMMON_API Log {
public:
    typedef boost::ptr_map<std::string, Appender> AppenderMap;
    typedef boost::ptr_map<std::string, Logger> LoggerMap;
    typedef std::function<Appender* (std::string, LogLevel, AppenderFlags, const StringVector&)> AppenderCreator;
    typedef std::map<std::string, AppenderCreator> AppenderCreatorMap;

    static void initialize();
    static void close();

    template <typename T>
    static void registerAppender() {
        std::string type = boost::mpl::c_str<typename T::AppenderType>::value;
        ASSERT(_appenderFactory.find(type) == _appenderFactory.end());
        _appenderFactory[type] = AppenderFactory<T>();
    }

    template <typename T>
    static void registerSink() {
        registerAppender<AppenderCustom<T>>();
    }

    static Logger* getLogger(const std::string &name) {
        auto iter = _loggers.find(name);
        return iter == _loggers.end() ? nullptr : iter->second;
    }

    static const std::string& getLogsDir() {
        return _logsDir;
    }

    template <typename... Args>
    static void debug(const char *format, Args&&... args) {
        ASSERT(_mainLogger != nullptr);
        _mainLogger->debug(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(const char *format, Args&&... args) {
        ASSERT(_mainLogger != nullptr);
        _mainLogger->info(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warn(const char *format, Args&&... args) {
        ASSERT(_mainLogger != nullptr);
        _mainLogger->warn(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(const char *format, Args&&... args) {
        ASSERT(_mainLogger != nullptr);
        _mainLogger->error(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void fatal(const char *format, Args&&... args) {
        ASSERT(_mainLogger != nullptr);
        _mainLogger->fatal(format, std::forward<Args>(args)...);
    }
protected:
    static void _loadFromConfig();
    static void _readAppendersFromConfig();
    static void _readLoggersFromConfig();
    static void _createAppenderFromConfig(const std::string &appenderName);
    static void _createLoggerFromConfig(const std::string &loggerName);
    static void _createSystemLoggers();

    static AppenderCreatorMap _appenderFactory;
    static AppenderMap _appenders;
    static LoggerMap _loggers;
    static std::string _logsDir;
    static Logger* _mainLogger;
};

#endif //TINYCORE_LOG_H
