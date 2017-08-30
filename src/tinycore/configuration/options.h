//
// Created by yuwenyong on 17-4-14.
//

#ifndef TINYCORE_OPTIONS_H
#define TINYCORE_OPTIONS_H

#include "tinycore/common/common.h"
#include <iostream>
#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include "tinycore/common/errors.h"


namespace po = boost::program_options;

class TC_COMMON_API OptionParser {
public:
    template <typename T>
    struct IsVector: public std::false_type {

    };

    template <typename T, typename A>
    struct IsVector<std::vector<T, A>>: public std::true_type {

    };

    explicit OptionParser(const std::string &caption)
            : _opts(caption) {
        define("version,v", "print version string");
        define("help,h", "display help message");
    }

    void define(const char *name, const char *help, const char *group=nullptr) {
        if (group != nullptr) {
            auto opt = getGroup(group);
            opt->add_options()(name, help);
        } else {
            _opts.add_options()(name, help);
        }
    }

    template <typename ArgT>
    void define(const char *name, const char *help, const boost::optional<ArgT> &defaultValue=boost::none,
                boost::function1<void, const ArgT&> callback= {}, const char *group= nullptr) {
        po::typed_value<ArgT> *value= po::value<ArgT>();
        if (defaultValue) {
            value->default_value(defaultValue.get());
        }
        if (callback) {
            value->notifier(std::move(callback));
        }
        if (group) {
            auto opt = getGroup(group);
            opt->add_options()(name, value, help);
        } else {
            _opts.add_options()(name, value, help);
        }
    }

    template <typename ArgT>
    void defineMulti(const char *name, const char *help, boost::function1<void, const std::vector<ArgT>&> callback= {},
                     const char *group= nullptr) {
        po::typed_value<std::vector<ArgT>> *value= po::value<std::vector<ArgT>>();
        if (callback) {
            value->notifier(std::move(callback));
        }
        value->composing();
        if (group) {
            auto opt = getGroup(group);
            opt->add_options()(name, value, help);
        } else {
            _opts.add_options()(name, value, help);
        }
    }


    bool has(const char *name) const {
        return _vm.count(name) != 0;
    }

    template <typename ValueT>
    const ValueT& get(const char *name) const {
        if (!has(name)) {
            ThrowException(KeyError, std::string("Unrecognized option ") + name);
        }
        auto &value = _vm[name];
        return value.as<ValueT>();
    }

    void parseCommandLine(int argc, const char * const argv[], bool final=true);

    void parseConfigFile(const char *path, bool final=true);

    void praseEnvironment(const boost::function1<std::string, std::string> &name_mapper, bool final=true);

    template <typename CallbackT>
    void addParseCallback(CallbackT &&calback) {
        _parseCallbacks.emplace_back(std::forward<CallbackT>(calback));
    }

    static OptionParser * instance();
protected:
    void runParseCallbacks() const {
        for (auto &callback: _parseCallbacks) {
            callback();
        }
    }

    po::options_description* getGroup(std::string group);

    po::options_description composeOptions() const;

    void helpCallback() {
        std::cout << composeOptions() << std::endl;
        exit(1);
    }

    void versionCallback() {
        std::cout << TINYCORE_VER << std::endl;
        exit(2);
    }

    po::options_description _opts;
    boost::ptr_map<std::string, po::options_description> _groups;
    po::variables_map _vm;
    std::vector<std::function<void()>> _parseCallbacks;
};

#define sOptions OptionParser::instance()

#endif //TINYCORE_OPTIONS_H
