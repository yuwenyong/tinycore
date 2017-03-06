//
// Created by yuwenyong on 17-2-7.
//

#include "tinycore/debugging/trace.h"


void Trace::assertHandler(const char *file, int line, const char *function, const char *message) {
    std::string error;
    error = String::format("%s:%i in %s ASSERTION FAILED:\n    %s", file, line, function, message);
    fprintf(stderr, "\n%s\n", error.c_str());
    fflush(stderr);
    throw AssertionError(error);
}


