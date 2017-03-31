//
// Created by yuwenyong on 17-2-7.
//

#include "tinycore/debugging/trace.h"


void Trace::assertHandler(const char *file, int line, const char *function, const char *message) {
    fprintf(stderr, "\n%s:%i in %s ASSERTION FAILED:\n    %s\n", file, line, function, message);
    fflush(stderr);
    ExceptionHelper::throwException<AssertionError>(file, line, function, message);
}


