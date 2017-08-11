//
// Created by yuwenyong on 17-3-30.
//

#ifndef TINYCORE_ERRORS_H
#define TINYCORE_ERRORS_H

#include <exception>
#include <stdexcept>
#include "tinycore/utilities/string.h"


class TC_COMMON_API SystemExit {
public:
    SystemExit(const char *file, int line, const char *func, const std::string &message="")
            : _file(file)
            , _line(line)
            , _func(func)
            , _message(message) {

    }

    const char *what() const;
protected:
    const char *_file;
    int _line;
    const char *_func;
    std::string _message;
    mutable std::string _what;
};


class TC_COMMON_API BaseException: public std::runtime_error {
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
class TC_COMMON_API Exception: public ParentException { \
public: \
    using ParentException::ParentException; \
    const char *getTypeName() const override { \
        return #Exception; \
    } \
}


DECLARE_EXCEPTION(Exception, BaseException);
DECLARE_EXCEPTION(KeyError, Exception);
DECLARE_EXCEPTION(IndexError, Exception);
DECLARE_EXCEPTION(TypeError, Exception);
DECLARE_EXCEPTION(ValueError, Exception);
DECLARE_EXCEPTION(IllegalArguments, Exception);
DECLARE_EXCEPTION(EnvironmentError, Exception);
DECLARE_EXCEPTION(IOError, EnvironmentError);
DECLARE_EXCEPTION(EOFError, Exception);
DECLARE_EXCEPTION(DuplicateKey, Exception);
DECLARE_EXCEPTION(NotFound, Exception);
DECLARE_EXCEPTION(MemoryError, Exception);
DECLARE_EXCEPTION(NotImplementedError, Exception);
DECLARE_EXCEPTION(TimeoutError, Exception);
DECLARE_EXCEPTION(ZeroDivisionError, Exception);
DECLARE_EXCEPTION(ParsingError, Exception);


class TC_COMMON_API AssertionError: public Exception {
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


class TC_COMMON_API ExceptionHelper {
public:
    template <typename Error, typename... Args>
    static void throwException(const char *file, int line, const char *func, Args&&... args) {
        throw Error(file, line, func, std::forward<Args>(args)...);
    }

    template <typename Error, typename... Args>
    static Error makeException(const char *file, int line, const char *func, Args&&... args) {
        return Error(file, line, func, std::forward<Args>(args)...);
    }

    template <typename Error, typename... Args>
    static std::exception_ptr makeExceptionPtr(const char *file, int line, const char *func, Args&&... args) {
        return std::make_exception_ptr(Error(file, line, func, std::forward<Args>(args)...));
    }
};

#define ThrowException(Exception, ...) ExceptionHelper::throwException<Exception>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define MakeException(Exception, ...) ExceptionHelper::makeException<Exception>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define MakeExceptionPtr(Exception, ...) ExceptionHelper::makeExceptionPtr<Exception>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#endif //TINYCORE_ERRORS_H
