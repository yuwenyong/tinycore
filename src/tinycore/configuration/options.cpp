//
// Created by yuwenyong on 17-4-14.
//

#include "tinycore/configuration/options.h"
#include <boost/functional/factory.hpp>


void Options::parseCommandLine(int argc, const char * const argv[], bool final) {
    auto opts = composeOptions();
    po::store(po::parse_command_line(argc, argv, opts), _vm);
    if (contain("help")) {
        helpCallback();
    }
    if (contain("version")) {
        versionCallback();
    }
    if (final) {
        po::notify(_vm);
        runParseCallbacks();
    }
}

void Options::parseConfigFile(const char *path, bool final) {
    auto opts = composeOptions();
    po::store(po::parse_config_file(path, opts, true), _vm);
    if (contain("help")) {
        helpCallback();
    }
    if (contain("version")) {
        versionCallback();
    }
    if (final) {
        po::notify(_vm);
        runParseCallbacks();
    }
}

Options* Options::instance() {
    static Options instance("Allowed options");
    return &instance;
}

po::options_description* Options::getGroup(std::string group) {
    auto iter = _groups.find(group);
    if (iter != _groups.end()) {
        return iter->second;
    } else {
        po::options_description *options = boost::factory<po::options_description *>()(group);
        _groups.insert(group, options);
        return options;
    }
}

po::options_description Options::composeOptions() const {
    po::options_description opts(_opts);
    for (auto &group: _groups) {
        opts.add(*group->second);
    }
    return opts;
}
