//
// Created by yuwenyong on 17-3-30.
//

#ifndef TINYCORE_STRING_H
#define TINYCORE_STRING_H

#include "tinycore/common/common.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#ifdef HAS_CPP_FORMAT
#include "fmt/format.h"
#endif

#ifdef HAS_RAPID_JSON
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#endif


class TC_COMMON_API String {
public:

    typedef std::tuple<std::string, std::string, std::string> PartitionResult;

    static std::string format(const char *fmt) {
        return std::string(fmt);
    }

#ifdef HAS_CPP_FORMAT
    template<typename... Args>
    static std::string format(const char *fmt, Args&&... args) {
        return fmt::sprintf(fmt, std::forward<Args>(args)...);
    }
#else
    template <typename... Args>
    static std::string format(const char *fmt, Args&&... args) {
        boost::format formatter(fmt);
        format(formatter, std::forward<Args>(args)...);
        return formatter.str();
    }
#endif

    template <typename... Args>
    static std::string formats(const char *fmt) {
        return std::string(fmt);
    }

    template <typename... Args>
    static std::string formats(const char *fmt, Args&&... args) {
        boost::format formatter(fmt);
        formats(formatter, std::forward<Args>(args)...);
        return formatter.str();
    }

    static StringVector split(const std::string &s, bool keepEmpty=true) {
        StringVector result;
        boost::split(result, s, boost::is_space(), keepEmpty?boost::token_compress_off:boost::token_compress_on);
        return result;
    }

    static StringVector split(const std::string &s, char delim, bool keepEmpty=true) {
        StringVector result;
        boost::split(result, s, [delim](char c) {
            return c == delim;
        }, keepEmpty?boost::token_compress_off:boost::token_compress_on);
        return result;
    }

    static StringVector split(const std::string &s, const std::string &delim, bool keepEmpty=true);
    static StringVector splitLines(const std::string &s, bool keepends=false);
    static PartitionResult partition(const std::string &s, const std::string &sep);
    static PartitionResult rpartition(const std::string &s, const std::string &sep);

    static size_t count(const std::string &s, char c, size_t start=0, size_t len=0);
    static size_t count(const std::string &s, const char *sub, size_t start=0, size_t len=0);
    static size_t count(const std::string &s, std::function<bool (char)> pred);

    static void capitalize(std::string &s);
    static std::string capitalizeCopy(const std::string &s);

    static std::string formatUTCDate(bool usegmt=false) {
        DateTime now = boost::posix_time::second_clock::universal_time();
        return formatUTCDate(now, usegmt);
    }

    static std::string formatUTCDate(const DateTime &timeval, bool usegmt=false);
    static std::string translate(const std::string &s, const std::array<char, 256> &table,
                                 const std::vector<char> &deleteChars);

    static std::string translate(const std::string &s, const std::array<char, 256> &table) {
        std::vector<char> deleteChars;
        return translate(s, table, deleteChars);
    }

    static bool toBool(const std::string &s) {
        std::string lowerStr = boost::to_lower_copy(s);
        return lowerStr == "1" || lowerStr == "true" || lowerStr == "yes";
    }

    static std::string toHexStr(const ByteArray &s, bool reverse= false);
    static ByteArray fromHexStr(const std::string &s, bool reverse= false);
    static std::string filter(const std::string &s, std::function<bool (char)> pred);

    static std::string fromJSON(const boost::property_tree::ptree &doc) {
        std::ostringstream buffer;
        boost::property_tree::write_json(buffer, doc);
        return buffer.str();
    }

#ifdef HAS_RAPID_JSON
    static std::string fromJSON(const rapidjson::Document &doc) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }
#endif

protected:
#ifndef HAS_CPP_FORMAT
    template <typename T, typename... Args>
    static void format(boost::format &formatter, T &&value, Args&&... args) {
        formatter % value;
        format(formatter, std::forward<Args>(args)...);
    }

    template <typename T>
    static void format(boost::format &formatter, T &&value) {
        formatter % value;
    }
#endif
    template <typename... Args>
    static void formats(boost::format &formatter, const char *value, Args&&... args) {
        formatter % value;
        format(formatter, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void formats(boost::format &formatter, const std::string &value, Args&&... args) {
        formatter % value;
        format(formatter, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    static void formats(boost::format &formatter, T &&value, Args&&... args) {
        formatter % std::to_string(value);
        format(formatter, std::forward<Args>(args)...);
    }

    static void formats(boost::format &formatter, const char *value) {
        formatter % value;
    }

    static void formats(boost::format &formatter, const std::string &value) {
        formatter % value;
    }

    template <typename T>
    static void formats(boost::format &formatter, T &&value) {
        formatter % std::to_string(value);
    }
};

#endif //TINYCORE_STRING_H
