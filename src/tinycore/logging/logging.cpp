//
// Created by yuwenyong on 17-8-8.
//

#include "tinycore/logging/logging.h"
#include <boost/functional/factory.hpp>
#include <boost/log/utility/exception_handler.hpp>


std::mutex Logging::_lock;
Logging::LoggerMap Logging::_loggers;
Logger* Logging::_rootLogger = nullptr;

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
    std::ifstream file(fileName);
    onPreInit();
    logging::init_from_stream(file);
    onPostInit();
}

void Logging::onPreInit() {
    logging::core::get()->set_exception_handler(logging::make_exception_suppressor());
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Uptime", attrs::timer());
    logging::register_formatter_factory("TimeStamp", boost::make_shared<TimeStampFormatterFactory>());
    logging::register_formatter_factory("Uptime", boost::make_shared<UptimeFormatterFactory>());
    logging::register_filter_factory("Channel", boost::make_shared<ChannelFilterFactory>());
    logging::register_sink_factory("File", boost::make_shared<FileSinkFactory>());
}

void Logging::onPostInit() {
    _rootLogger = getLogger();
}

Logger* Logging::getLoggerByName(const std::string &loggerName) {
    std::lock_guard<std::mutex> lock(_lock);
    auto iter  = _loggers.find(loggerName);
    if (iter != _loggers.end()) {
        return iter->second;
    }
    Logger *logger = boost::factory<Logger*>()(loggerName);
    _loggers.insert(loggerName, logger);
    return logger;
}