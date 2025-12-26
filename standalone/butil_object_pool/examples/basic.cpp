#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "butil_object_pool/object_pool.h"

struct MyObject {
    int x;
    std::string s;

    MyObject() : x(0), s() {}
    explicit MyObject(int v) : x(v), s("hello") {}
};

int main() {
    constexpr int kThreadCount = 4;
    constexpr int kIters = 10000;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([]() {
            for (int i = 0; i < kIters; ++i) {
                MyObject* obj = butil_object_pool::get_object<MyObject>(123);
                if (obj == nullptr) {
                    std::cerr << "get_object failed\n";
                    std::abort();
                }
                obj->x += 1;
                if (butil_object_pool::return_object(obj) != 0) {
                    std::cerr << "return_object failed\n";
                    std::abort();
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << butil_object_pool::describe_objects<MyObject>() << '\n';
    return 0;
}
