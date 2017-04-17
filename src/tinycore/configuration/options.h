//
// Created by yuwenyong on 17-4-14.
//

#ifndef TINYCORE_OPTIONS_H
#define TINYCORE_OPTIONS_H

#include "tinycore/common/common.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

class TC_COMMON_API Options {
public:
    Options();

    void define(const char* name, const char* help) {
        _opts.add_options()(name, help);
    }

    template <class T>
    void define(const char* name, T *arg, const char *help) {
        _opts.add_options()(std::move(name), po::value<T>(arg), std::move(help));
    }

    template <class T>
    void define(const char* name, T&& def, T *arg, const char *help) {
        _opts.add_options()(std::move(name), po::value<T>(arg)->default_value(std::forward<T>(def)), std::move(help));
    }

    void parseCommandLine(int argc, const char * const argv[]);
    void onExit();

    static Options * instance();
protected:
    po::options_description _opts;
    std::string _configFile;
};

#define sOptions Options::instance()

class _OptionsGuard {
public:
    _OptionsGuard(int argc, const char * const argv[]) {
        sOptions->parseCommandLine(argc, argv);
    }

    ~_OptionsGuard() {
        sOptions->onExit();
    }
};

#define ParseCommandLine(argc, argv) _OptionsGuard _opts(argc, argv)

#endif //TINYCORE_OPTIONS_H
