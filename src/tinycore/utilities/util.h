//
// Created by yuwenyong on 17-9-6.
//

#ifndef TINYCORE_UTIL_H
#define TINYCORE_UTIL_H

#include "tinycore/common/common.h"
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#ifdef HAS_RAPID_JSON
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#endif


inline std::string BytesToString(const ByteArray &bytes) {
    return {(const char *)bytes.data(), bytes.size()};
}

inline ByteArray StringToBytes(const std::string &s) {
    return ByteArray((const Byte *)s.data(), (const Byte *)s.data() + s.size());
}

inline bool StringToBool(const std::string &s) {
    std::string lowerStr = boost::to_lower_copy(s);
    return lowerStr == "1" || lowerStr == "true" || lowerStr == "yes";
}


class JsonUtil {
public:
    static std::string encode(const boost::property_tree::ptree &doc) {
        std::ostringstream buffer;
        boost::property_tree::write_json(buffer, doc);
        return buffer.str();
    }

    static void decode(const std::string &s, boost::property_tree::ptree &doc) {
        std::istringstream buffer(s);
        boost::property_tree::read_json(s, doc);
    }

#ifdef HAS_RAPID_JSON
    static std::string encode(const rapidjson::Document &doc) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }

    static void decode(const std::string &s, rapidjson::Document &doc) {
        doc.Parse(s.c_str(), s.size());
    }
#endif
};


class DateTimeUtil {
public:
    static std::string formatDate(bool usegmt=false) {
        DateTime now = boost::posix_time::second_clock::universal_time();
        return formatDate(now, usegmt);
    }

    static std::string formatDate(const DateTime &timeval, bool usegmt=false);

    static std::string formatUTCDate(const DateTime &ts);

    static DateTime parseUTCDate(const std::string &date);
};


class BinAscii {
public:
    static std::string b2aHex(const ByteArray &s, bool reverse= false);

    static ByteArray a2bHex(const std::string &s, bool reverse= false);
};


template <typename T>
class PtrVector: public boost::ptr_vector<T> {
public:
    typedef boost::ptr_vector<T> SuperType;
    using boost::ptr_vector<T>::ptr_vector;

    PtrVector(): SuperType() {

    }

    PtrVector(std::initializer_list<T*> il)
            : SuperType() {
        for (auto p: il) {
            SuperType::push_back(p);
        }
    }

    PtrVector(PtrVector &&r)
            : SuperType() {
        SuperType::swap(r);
    }

    PtrVector& operator=(PtrVector &&r) {
        SuperType::clear();
        SuperType::swap(r);
        return *this;
    }
};


#endif //TINYCORE_UTIL_H
