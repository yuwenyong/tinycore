//
// Created by yuwenyong on 17-3-30.
//

#ifndef TINYCORE_ERRORS_H
#define TINYCORE_ERRORS_H

#include <exception>
#include <stdexcept>
#include "tinycore/utilities/string.h"


class BaseException: public std::runtime_error {
public:
    BaseException(const char *file, int line, const char *func, const std::string &message)
            : std::runtime_error(message)
            , _file(file)
            , _line(line)
            , _func(func) {

    }

    const char *what() const noexcept override;

    virtual const char *getTypeName() const {
        return "BaseException";
    }
protected:
    const char *_file;
    int _line;
    const char *_func;
    mutable std::string _what;
};


#define DECLARE_EXCEPTION(Exception, ParentException) \
class Exception: public ParentException { \
public: \
    using ParentException::ParentException; \
    const char *getTypeName() const override { \
        return #Exception; \
    } \
}

DECLARE_EXCEPTION(SystemExit, BaseException);
DECLARE_EXCEPTION(Exception, BaseException);
DECLARE_EXCEPTION(KeyError, Exception);
DECLARE_EXCEPTION(IndexError, Exception);
DECLARE_EXCEPTION(TypeError, Exception);
DECLARE_EXCEPTION(ValueError, Exception);
DECLARE_EXCEPTION(IllegalArguments, Exception);
DECLARE_EXCEPTION(Environment, Exception);
DECLARE_EXCEPTION(IOError, Environment);
DECLARE_EXCEPTION(ParsingError, Exception);
DECLARE_EXCEPTION(DuplicateKey, Exception);
DECLARE_EXCEPTION(NotFound, Exception);


class AssertionError: public Exception {
public:
    using Exception::Exception;

    AssertionError(const char *file, int line, const char *func, const std::string &message,
                   const std::string &extra)
            : Exception(file, line, func, message)
            , _extra(extra) {

    }

    const char *what() const noexcept override;

    virtual const char *getTypeName() const override {
        return "AssertionError";
    }
protected:
    std::string _extra;
};


class ExceptionHelper {
public:
    template <typename Exception, typename... Args>
    static void throwException(const char *file, int line, const char *func, Args&&... args) {
        throw Exception(file, line, func, std::forward<Args>(args)...);
    }
};

#define ThrowException(Exception, ...) ExceptionHelper::throwException<Exception>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#endif //TINYCORE_ERRORS_H
