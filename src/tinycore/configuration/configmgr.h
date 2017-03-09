//
// Created by yuwenyong on 17-3-2.
//

#ifndef TINYCORE_CONFIGMGR_H
#define TINYCORE_CONFIGMGR_H

#include "tinycore/common/common.h"
#include <mutex>
#include <boost/property_tree/ptree.hpp>

class TC_COMMON_API ConfigMgr {
public:
    typedef boost::property_tree::ptree ConfigType;
    typedef ConfigType::path_type ConfigPathType;
//    typedef ConfigType::value_type ConfigValueType;

    ConfigMgr() = default;
    ConfigMgr(const ConfigMgr&) = delete;
    ConfigMgr& operator=(const ConfigMgr&) = delete;
    ~ConfigMgr() = default;

    void loadInitial(std::string fileName);

    void reload() {
        loadInitial(_fileName);
    }

    std::string const& GetFilename() {
        std::lock_guard<std::mutex> lock(_configLock);
        return _fileName;
    }

    std::string getString(const std::string &name) const;
    std::string getString(const std::string &name, const std::string &def) const;
    bool getBool(const std::string &name) const;
    bool getBool(const std::string &name, bool def) const;
    int getInt(const std::string &name) const;
    int getInt(const std::string &name, int def) const;
    float getFloat(const std::string &name) const;
    float getFloat(const std::string &name, float def) const;
    std::list<std::string> getKeysByString(const std::string &name);

    void setString(const std::string &name, const std::string &value) {
        setValue(name, value);
    }

    void setBool(const std::string &name, bool value) {
        setValue(name, value ? "true" : "false");
    }

    void setInt(const std::string &name, int value) {
        setValue(name, value);
    }

    void setFloat(const std::string &name, float value) {
        setValue(name, value);
    }

    static ConfigMgr* instance();
protected:
    template<typename T>
    T getValue(const std::string &name) const;
    template<typename T>
    T getValue(const std::string &name, T def) const;

    template<typename T>
    void setValue(const std::string &name, T value) {
        std::lock_guard<std::mutex> lock(_configLock);
        _config.put(ConfigPathType(name, '/'), value);
    }

    std::string _fileName;
    ConfigType _config;
    std::mutex _configLock;
};

#define sConfigMgr ConfigMgr::instance()
#endif //TINYCORE_CONFIGMGR_H
