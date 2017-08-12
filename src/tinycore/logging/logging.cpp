//
// Created by yuwenyong on 17-8-8.
//

#include "tinycore/logging/logging.h"
#include <boost/functional/factory.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include "tinycore/common/errors.h"


std::mutex Logging::_lock;
Logging::LoggerMap Logging::_loggers;
Logger* Logging::_rootLogger = nullptr;
const std::map<std::string, LogLevel> Logging::_severityMapping = {
        {"trace",   LOG_LEVEL_TRACE},
        {"debug",   LOG_LEVEL_DEBUG},
        {"info",    LOG_LEVEL_INFO},
        {"warning", LOG_LEVEL_WARNING},
        {"error",   LOG_LEVEL_ERROR},
        {"fatal",   LOG_LEVEL_FATAL},
};


void Logging::init() {
    onPreInit();
    onPostInit();
}

void Logging::initFromSettings(const Settings &settings) {
    onPreInit();
    logging::init_from_settings(settings);
    onPostInit();
}

void Logging::initFromFile(const std::string &fileName) {
    onPreInit();
    std::ifstream file(fileName);
    logging::init_from_stream(file);
    onPostInit();
}

LogLevel Logging::toSeverity(std::string severity) {
    boost::to_lower(severity);
    auto iter  = _severityMapping.find(severity);
    if (iter == _severityMapping.end()) {
        ThrowException(ValueError, "Invalid severity name:" + severity);
    }
    return iter->second;
}

void Logging::onPreInit() {
    if (isInitialized()) {
        return;
    }
    logging::core::get()->set_exception_handler(logging::make_exception_suppressor());
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Uptime", attrs::timer());
    logging::register_simple_formatter_factory<logging::trivial::severity_level , char>("Severity");
    logging::register_formatter_factory("TimeStamp", boost::make_shared<TimeStampFormatterFactory>());
    logging::register_formatter_factory("Uptime", boost::make_shared<UptimeFormatterFactory>());
    logging::register_simple_filter_factory<logging::trivial::severity_level , char>("Severity");
    logging::register_filter_factory("Channel", boost::make_shared<ChannelFilterFactory>());
    logging::register_sink_factory("File", boost::make_shared<FileSinkFactory>());
}

void Logging::onPostInit() {
    if (isInitialized()) {
        return;
    }
    _rootLogger = getLogger();
}

Logger* Logging::getLoggerByName(std::string loggerName) {
    std::lock_guard<std::mutex> lock(_lock);
    auto iter  = _loggers.find(loggerName);
    if (iter != _loggers.end()) {
        return iter->second;
    }
    Logger *logger = boost::factory<Logger*>()(loggerName);
    _loggers.insert(loggerName, logger);
    return logger;
}