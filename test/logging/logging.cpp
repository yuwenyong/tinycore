//
// Created by yuwenyong on 17-3-7.
//

#include "tinycore/logging/log.h"
#include "tinycore/configuration/configmgr.h"
#include <boost/mpl/string.hpp>


class TestSink: public BaseSink {
public:
    typedef boost::mpl::string<'T', 'e', 's', 't'> AppenderType;

    TestSink(const StringVector &extraArgs) {
        if (!extraArgs.empty()) {
            _arg = extraArgs.front();
        }
    }

    void onConsume(const DateTime &timeStamp, LogLevel logLevel, const std::string &channel,
                   const std::string &message, const std::string &format) {
        std::cout << '[' << _arg << ']'
                  << '[' <<  boost::gregorian::to_iso_string(timeStamp.date()) << ' '
                  << boost::posix_time::to_simple_string(timeStamp.time_of_day()) << ']'
                  << '[' << channel << ']'
                  << '<' << message << '>'
                  << std::endl;
        std::cout << ">>" << format << std::endl;
    }
protected:
    std::string _arg{"TestSink"};
};


int main() {
    sConfigMgr->setString("LogsDir", "logs");
    sConfigMgr->setString("Appender.Console", "Console,2,7");
    sConfigMgr->setString("Appender.File", "File,1,3,test.log");
    sConfigMgr->setString("Appender.Test", "Test,1,7,MySink");
    sConfigMgr->setString("Logger.logger1", "1,Console File");
    sConfigMgr->setString("Logger.logger2", "2,Test File");
    sConfigMgr->setString("Logger.logger3", "3,Console File Test");
    Log::registerSink<TestSink>();
    Log::initialize();
    Logger *logger1, *logger2, *logger3;
    logger1 = Log::getLogger("logger1");
    logger2 = Log::getLogger("logger2");
    logger3 = Log::getLogger("logger3");
    logger1->debug("debug msg from %s", "logger1");
    logger1->info("info msg from %s", "logger1");
    logger1->warn("warn msg from %s", "logger1");
    logger1->error("error msg from %s", "logger1");
    logger1->fatal("fatal msg from %s", "logger1");
    logger2->debug("debug msg from %s", "logger2");
    logger2->info("info msg from %s", "logger2");
    logger2->warn("warn msg from %s", "logger2");
    logger2->error("error msg from %s", "logger2");
    logger2->fatal("fatal msg from %s", "logger2");
    logger3->debug("debug msg from %s", "logger3");
    logger3->info("info msg from %s", "logger3");
    logger3->warn("warn msg from %s", "logger3");
    logger3->error("error msg from %s", "logger3");
    logger3->fatal("fatal msg from %s", "logger3");
    return 0;
}