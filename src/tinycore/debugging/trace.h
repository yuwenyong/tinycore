//
// Created by yuwenyong on 17-2-7.
//

#ifndef TINYCORE_TRACE_H
#define TINYCORE_TRACE_H

#include "tinycore/common/common.h"
#include "tinycore/common/errors.h"
#include "tinycore/utilities/string.h"


class TC_COMMON_API Trace {
public:
    template <typename... Args>
    static void warningHandler(char const* file, int line, char const* function, char const* message, Args&&... args) {
        std::string error;
        error = String::format("%s:%i in %s WARNING:\n    ", file, line, function);
        error += String::format(message, std::forward<Args>(args)...);
        fprintf(stderr, "\n%s\n", error.c_str());
        fflush(stderr);
    }

    static void assertHandler(const char *file, int line, const char *function, const char *message);

    template <typename... Args>
    static void assertHandler(const char *file, int line, const char *function, const char *message, const char *format,
                              Args&&... args) {
        std::string error;
        error = String::format(format, std::forward<Args>(args)...);
        fprintf(stderr, "\n%s:%i in %s ASSERTION FAILED:\n    %s\n    %s\n", file, line, function, message,
                error.c_str());
        fflush(stderr);
        ExceptionHelper::throwException<AssertionError>(file, line, function, message, std::move(error));
    }

    template <typename... Args>
    static void fatalHandler(const char *file, int line, const char *function, const char *message, Args&&... args) {
        std::string error;
        error = String::format(message, std::forward<Args>(args)...);
        fprintf(stderr, "\n%s:%i in %s FATAL ERROR:\n    %s\n", file, line, function, error.c_str());
        fflush(stderr);
//        std::this_thread::sleep_for(std::chrono::seconds(10));
        ExceptionHelper::throwException<SystemExit>(file, line, function, std::move(error));
    }
};

#if COMPILER == COMPILER_MICROSOFT
#define ASSERT_BEGIN __pragma(warning(push)) __pragma(warning(disable: 4127))
#define ASSERT_END __pragma(warning(pop))
#else
#define ASSERT_BEGIN
#define ASSERT_END
#endif


#define WARNING(cond, msg, ...) ASSERT_BEGIN do { if (!(cond)) Trace::warningHandler(__FILE__, __LINE__, __FUNCTION__, (msg), ##__VA_ARGS__); } while(0) ASSERT_END

#ifdef NDEBUG
#define ASSERT(cond, ...)
#else
#define ASSERT(cond, ...) ASSERT_BEGIN do { if (!(cond)) Trace::assertHandler(__FILE__, __LINE__, __FUNCTION__, #cond, ##__VA_ARGS__); } while(0) ASSERT_END
#endif

#define FATAL(cond, msg, ...) ASSERT_BEGIN do { if (!(cond)) Trace::fatalHandler(__FILE__, __LINE__, __FUNCTION__, (msg), ##__VA_ARGS__); } while(0) ASSERT_END


#endif //TINYCORE_TRACE_H
