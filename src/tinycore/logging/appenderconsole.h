//
// Created by yuwenyong on 17-3-6.
//

#ifndef TINYCORE_APPENDERCONSOLE_H
#define TINYCORE_APPENDERCONSOLE_H

#include "tinycore/logging/appender.h"
#include <boost/mpl/string.hpp>


class TC_COMMON_API AppenderConsole: public Appender {
public:
    typedef boost::mpl::string<'C', 'o', 'n', 's', 'o', 'l', 'e'> AppenderType;

    AppenderConsole(std::string name, LogLevel level, AppenderFlags flags, const StringVector &extraArgs)
            : Appender(name, level, flags) {

    }

protected:
    SinkTypePtr createSink() const override;
};

#endif //TINYCORE_APPENDERCONSOLE_H
