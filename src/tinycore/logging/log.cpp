//
// Created by yuwenyong on 17-2-4.
//

#include "tinycore/logging/log.h"
#include <boost/log/utility/setup.hpp>
#include <boost/functional/factory.hpp>
#include "tinycore/logging/appenderconsole.h"
#include "tinycore/logging/appenderfile.h"
#include "tinycore/configuration/configmgr.h"
#include "tinycore/utilities/string.h"
#include "tinycore/common/errors.h"


Log::AppenderCreatorMap Log::_appenderFactory;
Log::AppenderMap Log::_appenders;
Log::LoggerMap Log::_loggers;
std::string Log::_logsDir;
Logger* Log::_mainLogger = nullptr;


void Log::initialize() {
    registerAppender<AppenderConsole>();
    registerAppender<AppenderFile>();
    logging::add_common_attributes();
    loadFromConfig();
}

void Log::close() {
    _loggers.clear();
    _appenders.clear();
    auto core = logging::core::get();
    core->remove_all_sinks();
}

void Log::loadFromConfig() {
    close();
    _logsDir = sConfigMgr->getString("LogsDir", "");
    if (!_logsDir.empty()) {
        if (_logsDir[_logsDir.length() - 1] != '/' && _logsDir[_logsDir.length() - 1] != '\\') {
#if PLATFORM == PLATFORM_WINDOWS
            _logsDir.push_back('\\');
#else
            _logsDir.push_back('/');
#endif
        }
    }
    readAppendersFromConfig();
    readLoggersFromConfig();
    createSystemLoggers();
}



void Log::readAppendersFromConfig() {
    auto keys = sConfigMgr->getKeysByString("Appender.");
    while (!keys.empty()) {
        createAppenderFromConfig(keys.front());
        keys.pop_front();
    }
}

void Log::readLoggersFromConfig() {
    auto keys = sConfigMgr->getKeysByString("Logger.");
    while (!keys.empty()) {
        createLoggerFromConfig(keys.front());
        keys.pop_front();
    }
}

void Log::createAppenderFromConfig(const std::string &appenderName) {
    if (appenderName.empty()) {
        return;
    }
    std::string options = sConfigMgr->getString(appenderName);
    StringVector tokens = String::split(options, ',');
    std::string name = appenderName.substr(9);
    if (tokens.size() < 2) {
        std::string error = "Wrong config line " + options + " for appender " + name;
        ThrowException(ValueError, std::move(error));
    }
    AppenderFlags flags = APPENDER_FLAGS_NONE;
    std::string type = tokens[0];
    LogLevel level = static_cast<LogLevel>(std::stoi(tokens[1]));
    if (level > LOG_LEVEL_FATAL) {
        std::string error = "Wrong log level " + tokens[1] + " for appender " + name;
        ThrowException(ValueError, std::move(error));
    }
    size_t argOffset = 2;
    if (tokens.size() > 2) {
        flags = static_cast<AppenderFlags>(std::stoi(tokens[2]));
        ++argOffset;
    }
    auto factoryFunction = _appenderFactory.find(type);
    if (factoryFunction == _appenderFactory.end()) {
        std::string error = "Unknown type " + type + " for appender " + name;
        ThrowException(ValueError, std::move(error));
    }
    Appender *appender = factoryFunction->second(name, level, flags,
                                                 StringVector(std::next(tokens.begin(), argOffset), tokens.end()));
    _appenders.insert(name, appender);
}


void Log::createLoggerFromConfig(const std::string &loggerName) {
    if (loggerName.empty()) {
        return;
    }
    std::string options = sConfigMgr->getString(loggerName);
    std::string name = loggerName.substr(7);
    if (options.empty()) {
        std::string error = "Missing config option for logger " + name;
        ThrowException(ValueError, std::move(error));
    }
    StringVector tokens = String::split(options, ',');
    if (tokens.size() != 2) {
        std::string error = "Wrong config option " + options + " for logger " + name;
        ThrowException(ValueError, std::move(error));
    }
    if (_loggers.find(name) != _loggers.end()) {
        std::string error = "Config logger " + name + " already defined";
        ThrowException(DuplicateKey, std::move(error));
    }
    LogLevel level = static_cast<LogLevel>(std::stoi(tokens[0]));
    if (level > LOG_LEVEL_FATAL) {
        std::string error = "Wrong log level " + tokens[0] + " for logger " + name;
        ThrowException(ValueError, std::move(error));
    }
    Logger *logger = boost::factory<Logger*>()(name, level);
    _loggers.insert(name, logger);
    StringVector appenderNames = String::split(tokens[1], false);
    for (const auto &appenderName: appenderNames) {
        auto iter = _appenders.find(appenderName);
        if (iter == _appenders.end()) {
            std::string error = "Appender " + appenderName + "does not exist for logger " + name;
            ThrowException(NotFound, std::move(error));
        }
        iter->second->addLogger(logger);
    }
}

void Log::createSystemLoggers() {
    StringVector loggerNames = {"main", };
    size_t count = 0;
    for (const auto &loggerName: loggerNames) {
        if (_loggers.find(loggerName) == _loggers.end()) {
            ++count;
        }
    }
    if (count > 0) {
        std::string appenderType = boost::mpl::c_str<typename AppenderConsole::AppenderType>::value;
        std::string appenderName = appenderType;
        Appender *appender;
        auto iter = _appenders.find(appenderName);
        if (iter == _appenders.end()) {
            auto factoryFunction = _appenderFactory.find(appenderType);
            ASSERT(factoryFunction != _appenderFactory.end());
            AppenderFlags flags = static_cast<AppenderFlags>(APPENDER_FLAGS_TIMESTAMP|APPENDER_FLAGS_SEVERITY);
            appender = factoryFunction->second(appenderName, LOG_LEVEL_DEBUG, flags, {});
            _appenders.insert(appenderName, appender);
        } else {
            appender = iter->second;
        }
        for (auto &loggerName: loggerNames) {
            if (_loggers.find(loggerName) == _loggers.end()) {
                LogLevel level = LOG_LEVEL_INFO;
                Logger *logger = boost::factory<Logger*>()(loggerName, level);
                appender->addLogger(logger);
                _loggers.insert(loggerName, logger);
            }
        }
    }
    _mainLogger = getLogger("main");
}