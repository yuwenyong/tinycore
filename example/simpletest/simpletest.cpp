//
// Created by yuwenyong on 17-8-8.
//

#include <iostream>
#include <vector>

using namespace std;

struct MyCLS {
    template <typename... Args>
    void debug(const char *format0, const char *format, Args&&... args) {
        std::cout << "SecondVersion";
    }

    template <typename... Args>
    void debug(const char *format, Args&&... args) {
        std::cout << "FirstVersion";
    }
};

template <typename T>
void test(T val) {
    ++val;
}

class MyCLS2 {
public:
    template <typename T>
    T result(int i = 0) const {
        std::cout << "Template" << std::endl;
        result(i);
        return T();
    }

    int result(int i = 0) const {
        std::cout << "Non template" << std::endl;
        return 0;
    }
};

void change(int &a) {
    a = 2;
}

template <typename... Args>
void fchange(Args&&... args) {
    change(std::forward<Args>(args)...);
}


int main() {
    std::vector<unsigned char> data = {'\xe9'};
//    const char * s = "abc\0de";
    std::string ss((const char *)data.data(), data.size());
    printf("%d %d\n", (int)(ss.c_str()[0]), (int)(ss.c_str())[1]);
    printf("%s\n", ss.c_str());
    std::cout << ss.c_str() << std::endl;
    return 0;
}
