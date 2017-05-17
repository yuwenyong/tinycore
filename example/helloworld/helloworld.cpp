//
// Created by yuwenyong on 17-3-2.
//

#include <iostream>
#include <string>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "tinycore/debugging/trace.h"
#include "tinycore/networking/httpclient.h"


int main() {
    using JSONType = boost::property_tree::ptree;
//    JSONType json;
//    JSONType subJson, subJson2, subJson3;
////    json.add("my", "hehe");
////    json.add("my", "hehe2");
//    subJson2.put_value("hehe");
//    subJson.push_back({"", subJson2});
//    subJson2.put_value("hehe2");
//    subJson.push_back({"", subJson2});
//    json.add_child("my", subJson);
//    json.put("your", "dogs");
//    json.put("your2.girl", "xx");
////    subJson3.put("girl", "xx");
////    json.add_child("your2", subJson3);
////    const char * jsonStr = R"({
////        "my": [ "hehe", "hehe2" ],
////        "your": "dogs",
////        "your2": {
////            "girl": "xx"
////        }
////    })";
////    std::stringstream ss;
////    ss << jsonStr;
////    boost::property_tree::json_parser::read_json(ss, json);
//    boost::property_tree::json_parser::write_json("test.json", json);
//    std::cout << "pass 1" << std::endl;
//    for (const auto &v: json) {
////        std::cout << v.first << ":" << v.second.get_value<std::string>() << std::endl;
//    }
//    std::cout << "pass 2" << std::endl;
//    for (const auto &v: json.get_child("my")) {
////        std::cout << v.first << ":" << v.second.get_value<std::string>() << std::endl;
//    }

    HTTPRequest request1("testpath", authUserName_="HEHE"), request2("testPath2", maxRedirects_=5);
    request1.disp();
    request2.disp();
    return 0;
}