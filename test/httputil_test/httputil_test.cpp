//
// Created by yuwenyong on 17-6-2.
//

#define BOOST_TEST_MODULE httputil_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"


BOOST_AUTO_TEST_CASE(TestURLConcatNoQueryParams) {
    StringMap args = {{"y", "y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?y=y&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatEncodeArgs) {
    StringMap args = {{"y", "/y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?y=%2Fy&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatTrailingQ) {
    StringMap args = {{"y", "y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path?", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?y=y&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatQWithNoTrailingAmp) {
    StringMap args = {{"y", "y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path?x", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?x&y=y&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatTrailingAmp) {
    StringMap args = {{"y", "y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path?x&", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?x&y=y&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatMultParams) {
    StringMap args = {{"y", "y"}, {"z", "z"}};
    std::string url = HTTPUtil::urlConcat("https://localhost/path?a=1&b=2", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?a=1&b=2&y=y&z=z");
}

BOOST_AUTO_TEST_CASE(TestURLConcatNoParams) {
    StringMap args = {};
    std::string url = HTTPUtil::urlConcat("https://localhost/path?r=1&t=2", args);
    BOOST_CHECK_EQUAL(url, "https://localhost/path?r=1&t=2");
}