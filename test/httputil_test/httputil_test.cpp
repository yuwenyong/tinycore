//
// Created by yuwenyong on 17-6-2.
//

#define BOOST_TEST_MODULE httputil_test
#include <boost/test/included/unit_test.hpp>
#include "tinycore/tinycore.h"

TINYCORE_TEST_INIT()

using StringPairs = std::vector<std::pair<std::string, std::string>>;
BOOST_TEST_DONT_PRINT_LOG_VALUE(StringPairs)
BOOST_TEST_DONT_PRINT_LOG_VALUE(StringVector)

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

BOOST_AUTO_TEST_CASE(TestFileUpload) {
    const char *data = "--1234\r\nContent-Disposition: form-data; name=\"files\"; filename=\"ab.txt\"\r\n\r\nFoo\r\n--1234--";
    HTTPUtil::QueryArgumentsType args;
    HTTPUtil::RequestFilesType files;
    HTTPUtil::parseMultipartFormData("1234", ByteArray((const Byte *)data, (const Byte *)data + strlen(data)), args,
                                     files);
    auto &file = files["files"][0];
    BOOST_CHECK_EQUAL(file.getFileName(), "ab.txt");
    std::string body((const char*)file.getBody().data(), file.getBody().size());
    BOOST_CHECK_EQUAL(body, "Foo");
}

BOOST_AUTO_TEST_CASE(TestUnquotedNames) {
    const char *data = "--1234\r\nContent-Disposition: form-data; name=files; filename=ab.txt\r\n\r\nFoo\r\n--1234--";
    HTTPUtil::QueryArgumentsType args;
    HTTPUtil::RequestFilesType files;
    HTTPUtil::parseMultipartFormData("1234", ByteArray((const Byte *)data, (const Byte *)data + strlen(data)), args,
                                     files);
    auto &file = files["files"][0];
    BOOST_CHECK_EQUAL(file.getFileName(), "ab.txt");
    std::string body((const char*)file.getBody().data(), file.getBody().size());
    BOOST_CHECK_EQUAL(body, "Foo");
}

BOOST_AUTO_TEST_CASE(TestSpecialFilenames) {
    StringVector filenames = {
            "a;b.txt",
            "a\"b.txt",
            "a\";b.txt",
            "a;\"b.txt",
            "a\";\";.txt",
            "a\\\"b.txt",
            "a\\b.txt",
    };
    for (auto &filename: filenames) {
        Log::info("try filename %s", filename.c_str());
        std::string name = boost::replace_all_copy(boost::replace_all_copy(filename, "\\", "\\\\"), "\"", "\\\"");
        auto data = String::format("--1234\r\nContent-Disposition: form-data;"\
        " name=\"files\"; filename=\"%s\"\r\n\r\nFoo\r\n--1234--", name.c_str());
        HTTPUtil::QueryArgumentsType args;
        HTTPUtil::RequestFilesType files;
        HTTPUtil::parseMultipartFormData("1234", ByteArray((const Byte *)data.data(),
                                                           (const Byte *)data.data() + data.size()), args, files);
        auto &file = files["files"][0];
        BOOST_CHECK_EQUAL(file.getFileName(), filename);
        std::string body((const char*)file.getBody().data(), file.getBody().size());
        BOOST_CHECK_EQUAL(body, "Foo");
    }
}

BOOST_AUTO_TEST_CASE(TestMultiLine) {
    const char *data = "Foo: bar\r\n baz\r\nAsdf: qwer\r\n\tzxcv\r\nFoo: even\r\n     more\r\n     lines\r\n";
    auto headers = HTTPHeaders::parse(data);
    BOOST_CHECK_EQUAL(headers->at("asdf"), "qwer zxcv");
    BOOST_CHECK_EQUAL(headers->getList("asdf"), StringVector{"qwer zxcv"});
    BOOST_CHECK_EQUAL(headers->at("Foo"), "bar baz,even more lines");
    StringVector values{"bar baz", "even more lines"};
    BOOST_CHECK_EQUAL(headers->getList("foo"), values);
    StringPairs collectValues;
    headers->getAll([&collectValues](const std::string &key, const std::string &value){
        collectValues.emplace_back(key, value);
    });
    std::sort(collectValues.begin(), collectValues.end());
    const StringPairs targetValues = {{"Asdf", "qwer zxcv"},
                                      {"Foo",  "bar baz"},
                                      {"Foo",  "even more lines"}};
    BOOST_CHECK_EQUAL(collectValues, targetValues);
}