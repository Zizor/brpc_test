#include <cassert>
#include <cstdint>
#include <thread>
#include <vector>

#include "butil_object_pool/object_pool.h"

struct Pod {
    std::uint64_t v;
};

struct HasArgs {
    int a;
    int b;
    HasArgs(int x, int y) : a(x), b(y) {}
};

int main() {
    Pod* p = butil_object_pool::get_object<Pod>();
    assert(p != nullptr);
    assert(butil_object_pool::return_object(p) == 0);

    HasArgs* q = butil_object_pool::get_object<HasArgs>(1, 2);
    assert(q != nullptr);
    assert(q->a == 1 && q->b == 2);
    assert(butil_object_pool::return_object(q) == 0);

    constexpr int kThreadCount = 8;
    constexpr int kIters = 10000;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([]() {
            for (int i = 0; i < kIters; ++i) {
                Pod* obj = butil_object_pool::get_object<Pod>();
                assert(obj != nullptr);
                assert(butil_object_pool::return_object(obj) == 0);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    const butil_object_pool::ObjectPoolInfo info = butil_object_pool::describe_objects<Pod>();
    assert(info.block_num > 0);
    assert(info.item_num > 0);
    return 0;
}
