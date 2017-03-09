//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_APPENDERFILE_H
#define TINYCORE_APPENDERFILE_H

#include "tinycore/logging/appender.h"
#include <boost/mpl/string.hpp>


class TC_COMMON_API AppenderFile: public Appender {
public:
    typedef boost::mpl::string<'F', 'i', 'l', 'e'> AppenderType;

    AppenderFile(std::string name, LogLevel level, AppenderFlags flags, const StringVector &extraArgs);
protected:
    SinkTypePtr _createSink() const override;

    std::string _fileName;
    size_t _maxFileSize{5 * 1024 * 1024};
    std::ios_base::openmode _mode{std::ios_base::app|std::ios_base::out};
};


#endif //TINYCORE_APPENDERFILE_H
