//
// Created by yuwenyong on 17-4-14.
//

#include "tinycore/configuration/options.h"
#include <iostream>
#include "tinycore/configuration/configmgr.h"
#include "tinycore/logging/log.h"
#include "tinycore/utilities/objectmanager.h"


Options::Options()
        : _opts("Allowed options") {
    define("version,v", "print version string");
    define("help,h", "display help message");
    define("config,c", &_configFile, "specify config file");
}

void Options::parseCommandLine(int argc, const char * const argv[]) {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, _opts), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << _opts << std::endl;
        exit(1);
    }
    if (vm.count("version")) {
        std::cout << TINYCORE_VER << std::endl;
        exit(2);
    }
    if (vm.count("config")) {
        sConfigMgr->loadInitial(_configFile);
    }
    Log::initialize();
}

void Options::onExit() {
    sObjectMgr->finish();
    Log::close();
}

Options* Options::instance() {
    static Options instance;
    return &instance;
}