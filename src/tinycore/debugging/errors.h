//
// Created by yuwenyong on 17-3-3.
//

#ifndef TINYCORE_ERRORS_H
#define TINYCORE_ERRORS_H

#include "tinycore/common/common.h"


class BaseException: public std::runtime_error {
public:
    BaseException(const char *what_arg): std::runtime_error(what_arg) {

    }

    BaseException(const std::string &what_arg): std::runtime_error(what_arg) {

    }
};


class Exception: public BaseException {
public:
    using BaseException::BaseException;
};


class SystemExit: public BaseException {
public:
    using BaseException::BaseException;
};


class KeyError: public Exception {
public:
    using Exception::Exception;
};


class TypeError: public Exception {
public:
    using Exception::Exception;
};


class ValueError: public Exception {
public:
    using Exception::Exception;
};


class AssertionError: public Exception {
public:
    using Exception::Exception;
};


class ParsingError: public Exception {
public:
    using Exception::Exception;
};


#endif //TINYCORE_ERRORS_H
