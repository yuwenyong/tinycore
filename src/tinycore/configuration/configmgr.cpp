//
// Created by yuwenyong on 17-3-2.
//

#include "tinycore/configuration/configmgr.h"
#include <boost/property_tree/ini_parser.hpp>
#include "tinycore/common/errors.h"


void ConfigMgr::loadInitial(std::string fileName) {
    std::lock_guard<std::mutex> lock(_configLock);
    std::string error;
    try {
        ConfigType config;
        boost::property_tree::read_ini(fileName, config);
        if (config.empty()) {
            error = "empty file (" + fileName + ")";
            ThrowException(ParsingError, error);
        }
        _fileName = std::move(fileName);
        _config.swap(config);
    } catch (const boost::property_tree::ini_parser_error &e) {
        if (e.line() == 0) {
            error = e.message() + "(" + e.filename() + ")";
        } else {
            error = e.message() + "(" + e.filename() + ":" + std::to_string(e.line()) + ")";
        }
        ThrowException(ParsingError, error);
    }
}

std::string ConfigMgr::getString(const std::string &name) const {
    std::string val = getValue<std::string>(name);
    val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
    return val;
}

std::string ConfigMgr::getString(const std::string &name, const std::string &def) const {
    std::string val = getValue(name, def);
    val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
    return val;
}

bool ConfigMgr::getBool(const std::string &name) const {
    std::string val = getValue<std::string>(name);
    val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
    return (val == "1" || val == "true" || val == "TRUE" || val == "yes" || val == "YES");
}

bool ConfigMgr::getBool(const std::string &name, bool def) const {
    std::string val = getValue(name, std::string(def ? "1" : "0"));
    val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
    return (val == "1" || val == "true" || val == "TRUE" || val == "yes" || val == "YES");
}

int ConfigMgr::getInt(const std::string &name) const {
    return getValue<int>(name);
}

int ConfigMgr::getInt(const std::string &name, int def) const {
    return getValue(name, def);
}

float ConfigMgr::getFloat(const std::string &name) const {
    return getValue<float>(name);
}

float ConfigMgr::getFloat(const std::string &name, float def) const {
    return getValue(name, def);
}

std::list<std::string> ConfigMgr::getKeysByString(const std::string &name) {
    std::lock_guard<std::mutex> lock(_configLock);
    std::list<std::string> keys;
    for (const auto &child : _config) {
        if (child.first.compare(0, name.length(), name) == 0) {
            keys.push_back(child.first);
        }
    }
    return keys;
}

ConfigMgr* ConfigMgr::instance() {
    static ConfigMgr instance;
    return &instance;
}

template <typename T>
T ConfigMgr::getValue(const std::string &name) const {
    try {
        return _config.get<T>(ConfigPathType(name, '/'));
    } catch (boost::property_tree::ptree_bad_path) {
        std::string error;
        error = "Missing name " + name + " in config file " + _fileName;
        ThrowException(KeyError, error);
    } catch (boost::property_tree::ptree_bad_data) {
        std::string error;
        error = "Bad value defined for name " + name + " in config file " + _fileName;
        ThrowException(TypeError, error);
    }
}

template <typename T>
T ConfigMgr::getValue(const std::string &name, T def) const {
    return _config.get(ConfigPathType(name, '/'), def);
}