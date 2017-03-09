//
// Created by yuwenyong on 17-3-6.
//

#include "tinycore/logging/appender.h"
#include <boost/log/support/date_time.hpp>


Appender::~Appender() {

}


bool Appender::_filter(const logging::value_ref<LogLevel , tag::attr_severity> &level,
                       const logging::value_ref<std::string, tag::attr_channel> &channel) const {
    if (level < _level) {
        return false;
    }
    if (_loggers.find(channel.get()) == _loggers.end()) {
        return false;
    }
    return true;
}

void Appender::_formatter(const logging::record_view &rec, logging::formatting_ostream &strm) const {
    if (_flags & APPENDER_FLAGS_TIMESTAMP) {
        const DateTime *timeStamp = rec[attr_timestamp].get_ptr<DateTime>();
        strm << '[' << boost::gregorian::to_iso_extended_string(timeStamp->date()) << ' '
             << boost::posix_time::to_simple_string(timeStamp->time_of_day()) << ']';
    }
    if (_flags & APPENDER_FLAGS_SEVERITY) {
        strm << '[' << rec[attr_severity] << ']';
    }
    if (_flags & APPENDER_FLAGS_CHANNEL) {
        strm << '[' << rec[attr_channel] << ']';
    }
    strm << rec[expr::smessage];
}
