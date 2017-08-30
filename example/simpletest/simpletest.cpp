//
// Created by yuwenyong on 17-8-8.
//

#include <iostream>

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
    int x = 0;
    fchange(x);
    std::cerr << x << std::endl;
    return 0;
}
