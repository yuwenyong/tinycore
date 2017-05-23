//
// Created by yuwenyong on 17-3-2.
//

#include <iostream>
#include <string>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "tinycore/debugging/trace.h"
#include "tinycore/asyncio/httpclient.h"


int main() {
    auto request1 = HTTPRequest::create("testpath", authUserName_="HEHE");
    auto request2 = HTTPRequest::create(url_="testPath2", method_="GET", maxRedirects_=5);
    return 0;
}