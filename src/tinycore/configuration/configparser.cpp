//
// Created by yuwenyong on 17-8-11.
//

#include "tinycore/configuration/configparser.h"
#include <boost/property_tree/ini_parser.hpp>


void ConfigParser::read(const std::string &fileName) {
    std::string error;
    try {
        ConfigType config;
        boost::property_tree::read_ini(fileName, config);
        if (config.empty()) {
            error = "empty file (" + fileName + ")";
            ThrowException(ParsingError, std::move(error));
        }
        _config.swap(config);
    } catch (const boost::property_tree::ini_parser_error &e) {
        if (e.line() == 0) {
            error = e.message() + "(" + e.filename() + ")";
        } else {
            error = e.message() + "(" + e.filename() + ":" + std::to_string(e.line()) + ")";
        }
        ThrowException(ParsingError, std::move(error));
    }
}

void ConfigParser::write(const std::string &fileName) {
    boost::property_tree::write_ini(fileName, _config);
}

StringVector ConfigParser::getSections() const {
    StringVector sections;
    for (const auto &child : _config) {
        sections.emplace_back(child.first);
    }
    return sections;
}

bool ConfigParser::removeSection(const std::string &section) {
    auto iter = _config.find(section);
    if (iter == _config.not_found()) {
        return false;
    }
    _config.erase(_config.to_iterator(iter));
    return true;
}

StringVector ConfigParser::getOptions(const std::string &section, bool ignoreError) const {
    if (!hasSection(section)) {
        if (ignoreError) {
            return {};
        } else {
            ThrowException(NoSectionError, "No section: " + section);
        }
    }
    StringVector options;
    for (const auto &option: _config.get_child(section)) {
        options.emplace_back(option.first);
    }
    return options;
}

bool ConfigParser::hasOption(const std::string &section, const std::string &option) const {
    auto iter = _config.find(section);
    if (iter == _config.not_found()) {
        return false;
    }
    auto sectionIter = _config.to_iterator(iter);
    iter = sectionIter->second.find(option);
    return iter != sectionIter->second.not_found();
}

bool ConfigParser::removeOption(const std::string &section, const std::string &option) {
    auto iter = _config.find(section);
    if (iter == _config.not_found()) {
        return false;
    }
    auto sectionIter = _config.to_iterator(iter);
    iter = sectionIter->second.find(option);
    if (iter == sectionIter->second.not_found()) {
        return false;
    }
    auto optionIter = sectionIter->second.to_iterator(iter);
    sectionIter->second.erase(optionIter);
    return true;
}

StringMap ConfigParser::getItems(const std::string &section, bool ignoreError) const {
    if (!hasSection(section)) {
        if (ignoreError) {
            return {};
        } else {
            ThrowException(NoSectionError, "No section: " + section);
        }
    }
    StringMap items;
    for (const auto &item: _config.get_child(section)) {
        items.emplace(item.first, item.second.data());
    }
    return items;
}