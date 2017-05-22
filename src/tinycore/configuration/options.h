//
// Created by yuwenyong on 17-4-14.
//

#ifndef TINYCORE_OPTIONS_H
#define TINYCORE_OPTIONS_H

#include "tinycore/common/common.h"
#include <boost/program_options.hpp>
#include "tinycore/common/errors.h"


namespace po = boost::program_options;

class TC_COMMON_API Options {
public:
    Options();

    void define(const char *name, const char *help) {
        _opts.add_options()(name, help);
    }

    template <typename T>
    void define(const char *name, const char *help) {
        _opts.add_options()(name, po::value<T>(), help);
    }

    template <typename  T>
    void define(const char *name, T&& def, const char *help) {
        _opts.add_options()(name, po::value<T>()->default_value(std::forward<T>(def)), help);
    }

    void parseCommandLine(int argc, const char * const argv[]);

    bool contain(const char *name) const {
        return _vm.count(name) != 0;
    }

    template <typename T>
    const T& get(const char *name) const {
        if (!contain(name)) {
            std::string error = "Unrecognized option ";
            error += name;
            ThrowException(KeyError, std::move(error));
        }
        auto value = _vm[name];
        return value.as<T>();
    }

    void onEnter();
    void onExit();

    static Options * instance();
protected:
    void setupWatcherHook();

    po::options_description _opts;
    po::variables_map _vm;
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
