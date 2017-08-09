//
// Created by yuwenyong on 17-3-7.
//

#include <thread>
#include "tinycore/tinycore.h"


int main() {
    std::cout << "Init" << std::endl;
    Logging::Settings settings;
    settings["Sinks.Console"]["Destination"] = "Console";
    settings["Sinks.Console"]["Asynchronous"] = true;
    settings["Sinks.Console"]["Format"] = "ConsoleSink[%Severity%][%Channel%]%Message%";
    settings["Sinks.Console"]["AutoFlush"] = true;

    settings["Sinks.File"]["Destination"] = "File";
    settings["Sinks.File"]["Format"] = "ComSink[%Severity%][%Channel%]%Message%";
    settings["Sinks.File"]["FileName"] = "logs/test.log";
    settings["Sinks.File"]["Filter"] = "%Channel% child_of \"root.com\"";
    Logging::initFromSettings(settings);
    Logger *accessLogger = Logging::getLogger("access");
    Logger *appLogger = Logging::getLogger("app");
    Logger *comLogger = Logging::getLogger("com");
//    Logging::init();
    ConsoleSink sink;
    sink.setFilter("access", LOG_LEVEL_INFO);
    sink.setFormatter("AccessSink[%Severity%][%Channel%]%Message%");
    Logging::addSink(sink);

    ConsoleSink sink2;
    sink2.setFilter("%Channel% child_of \"root.com\"");
    sink2.setFormatter("[%TimeStamp(format=\"%Y.%m.%d %H:%M:%S.%f\")%][%Severity%][%File%:%Line%:%Func%]%Message%");
    Logging::addSink(sink2);

    RotatingFileSink sink3("logs/mylog_%3N.log");
    sink3.setFilter("app", LOG_LEVEL_INFO);
    sink3.setFormatter("AppSink[%Severity%][%Channel%]%Message%");
    Logging::addSink(sink3);

    ByteArray bytes;
    for (int i = 0; i < 100; ++i) { bytes.push_back((Byte)i); }
    LOG_ERROR("This is log %d", 1);
    LOG_ERROR(bytes.data(), bytes.size());
    LOG_ERROR(accessLogger, "This is log %d", 2);
    LOG_DEBUG(accessLogger, "This is log %d", 3);
    LOG_ERROR(accessLogger, bytes.data(), bytes.size());
    LOG_ERROR(appLogger, "This is log %d", 4);
    LOG_ERROR(comLogger, "This is log %d", 5);
    Logging::close();
    std::cout << "Closed" << std::endl;
//    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}