//
// Created by yuwenyong on 17-3-2.
//

#include "tinycore/tinycore.h"
#include <boost/noncopyable.hpp>


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


class Test: public boost::noncopyable {
public:
    Test(int i): _i(i) {}

    void display() const {
        std::cout << _i << std::endl;
    }
protected:
    int _i;
};


int main() {
//    std::exception_ptr e;
//    try {
//        ThrowException(IOError, "Fuck");
//    } catch (...) {
//        e = std::current_exception();
//    }
//    if (e) {
//        handleError(e);
//    }
    std::stringstream s;
    s.write("hehe", 4);
    auto length = s.tellp() - s.tellg();
    std::cout << "Length:" << length << std::endl;
    std::string v;
//    v.resize(length);
//    s.read((char *)v.data(), length);
//    std::cout << v << std::endl;
    s.ignore(length);
    if (s.eof()) {
        std::cout << "EOF" << std::endl;
    } else {
        std::cout << "NOT EOF" << std::endl;
    }
    length = s.tellp() - s.tellg();
    std::cout << "Length2:" << length << std::endl;

    s.write("HEHEH", 5);
    length = s.tellp() - s.tellg();
    std::cout << "Length3:" << length << std::endl;
    v.resize(length);
    s.read((char *)v.data(), length);
    std::cout << v << std::endl;
    length = s.tellp() - s.tellg();
    std::cout << "Length4:" << length << std::endl;

//    s.ignore(v.size());
//    s.clear();
//    std::cerr << v << std::endl;
//    if (s.eof()) {
//        std::cerr << "EOF" << std::endl;
//    } else {
//        v = s.str();
//        std::cerr << "NOT EOF" << v.size() << std::endl;
//    }
//    s.write("HAHA", 4);
//    v = s.str();
//    std::cerr << v << std::endl;
//    if (s.eof()) {
//        std::cerr << "EOF" << std::endl;
//    } else {
//
//        std::cerr << "NOT EOF" << v.size() << std::endl;
//    }
    return 0;
}