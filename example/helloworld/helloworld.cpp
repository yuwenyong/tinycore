//
// Created by yuwenyong on 17-3-2.
//

#include "tinycore/tinycore.h"


void handleError(std::exception_ptr e) {
    std::cerr << "HandleError" << std::endl;
    try {
        std::rethrow_exception(e);
    } catch (IOError &error) {
        std::cerr << error.what() << std::endl;
        std::cerr << "perfect" << std::endl;
    } catch (std::string &error) {
        std::cerr << error << std::endl;
    } catch (int &error) {
        std::cerr << error << std::endl;
    }
}

int main() {
    std::exception_ptr e;
    try {
        ThrowException(IOError, "Fuck");
    } catch (...) {
        e = std::current_exception();
    }
    if (e) {
        handleError(e);
    }
    return 0;
}