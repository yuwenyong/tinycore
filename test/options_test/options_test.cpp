//
// Created by yuwenyong on 17-8-18.
//

#define BOOST_TEST_MODULE options_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"

TINYCORE_TEST_INIT()

BOOST_TEST_DONT_PRINT_LOG_VALUE(std::vector<int>)

BOOST_AUTO_TEST_CASE(TestParseCommandLine) {
    OptionParser options("Allow Options");
    options.define<unsigned short>("port", "listen port", 80);
    const char *argv[] = {
            "main",
            "--port=443"
    };
    options.parseCommandLine(sizeof(argv) / sizeof(const char *), argv);
    BOOST_CHECK_EQUAL(options.get<unsigned short>("port"), 443);
}

BOOST_AUTO_TEST_CASE(TestParseConfigFile) {
    OptionParser options("Allow Options");
    options.define<unsigned short>("port", "listen port", 80);
    options.parseConfigFile("options_test.cfg");
    BOOST_CHECK_EQUAL(options.get<unsigned short>("port"), 443);
}

BOOST_AUTO_TEST_CASE(TestParseCallbacks) {
    OptionParser options("Allow Options");
    bool called = false;
    options.addParseCallback([&called]() {
        called = true;
    });
    const char *argv[] = {
            "main",
    };
    options.parseCommandLine(sizeof(argv) / sizeof(const char *), argv, false);
    BOOST_CHECK(!called);
    options.parseCommandLine(sizeof(argv) / sizeof(const char *), argv);
    BOOST_CHECK(called);
}

//BOOST_AUTO_TEST_CASE(TestHelp) {
//    OptionParser options("Allow Options");
//    const char *argv[] = {
//            "main",
//            "--help"
//    };
//    options.parseCommandLine(sizeof(argv) / sizeof(const char *), argv);
//}

BOOST_AUTO_TEST_CASE(TestSetAttrWithCallback) {
    std::vector<int> values, targetValues{2};
    OptionParser options("Allow Options");
    options.define<int>("foo", "foo", 1, [&values](const int &value) {
        values.push_back(value);
    });
    const char *argv[] = {
            "main",
            "--foo=2"
    };
    options.parseCommandLine(sizeof(argv) / sizeof(const char *), argv);
    BOOST_CHECK_EQUAL(values, targetValues);
}