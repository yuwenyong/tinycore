//
// Created by yuwenyong on 17-8-12.
//

#ifndef TINYCORE_GLOBALINIT_H
#define TINYCORE_GLOBALINIT_H

#include "tinycore/common/common.h"
#include <boost/function.hpp>


class TC_COMMON_API GlobalInit {
public:
    GlobalInit(const boost::function1<std::string, std::string> &name_mapper) {
        initFromEnvironment(name_mapper);
    }

    GlobalInit(int argc, const char * const argv[]) {
        initFromCommandLine(argc, argv);
    }

    explicit GlobalInit(const char *path) {
        initFromConfigFile(path);
    }

    ~GlobalInit() {
        cleanup();
    }

    static void initFromEnvironment(const boost::function1<std::string, std::string> &name_mapper={});
    static void initFromCommandLine(int argc, const char * const argv[]);
    static void initFromConfigFile(const char *path);
    static void cleanup();
protected:
    static void setupWatcherHook();

    static bool _inited;
};


#define ParseCommandLine(argc, argv)    GloablInit _initializer(argc, argv)
#define ParseConfigFile(fileName)       GloablInit _initializer(fileName)


#endif //TINYCORE_GLOBALINIT_H
