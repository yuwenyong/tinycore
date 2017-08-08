//
// Created by yuwenyong on 17-3-6.
//

#include "tinycore/logging/logger.h"
#include <boost/log/utility/manipulators.hpp>
#include "tinycore/logging/logging.h"


Logger* Logger::getChild(const std::string &suffix) const {
    return Logging::getChildLogger(this, suffix);
}


void Logger::write(LogLevel level, const Byte *data, size_t length) {
    logging::record rec = _logger.open_record(keywords::severity=level);
    if (rec) {
        logging::record_ostream strm(rec);
        strm << logging::dump(data, length);
        _logger.push_record(std::move(rec));
    }
}

void Logger::write(const StringLiteral &file, size_t line, const StringLiteral &func, LogLevel level, const Byte *data,
                   size_t length) {
    logging::record rec = _logger.open_record(keywords::severity=level, logger_keywords::file=file,
                                              logger_keywords::line=line, logger_keywords::func=func);
    if (rec) {
        logging::record_ostream strm(rec);
        strm << logging::dump(data, length);
        _logger.push_record(std::move(rec));
    }
}