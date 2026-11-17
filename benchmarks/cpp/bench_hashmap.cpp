// C++ reference for benchmarks/cdd/bench_hashmap.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    const int32_t cap = 16777216;
    const int32_t mask = cap - 1;
    const int32_t ops = 8000000;
    int32_t* keys = static_cast<int32_t*>(std::malloc(cap * 4));
    int32_t* vals = static_cast<int32_t*>(std::malloc(cap * 4));
    int32_t* probe = static_cast<int32_t*>(std::malloc(ops * 4));
    for (int32_t i = 0; i < cap; ++i) {
        keys[i] = -1;
        vals[i] = 0;
    }
    uint32_t seed = 987654321u;
    for (int32_t i = 0; i < ops; ++i) {
        seed = seed * 1664525u + 1013904223u;
        int32_t key = static_cast<int32_t>(seed & 0x7fffffffu);
        probe[i] = key;
        uint32_t h = static_cast<uint32_t>(key) * 2654435761u;
        int32_t idx = static_cast<int32_t>(h & 0xffffffu);
        while (keys[idx] != -1 && keys[idx] != key) idx = (idx + 1) & mask;
        if (keys[idx] == -1) {
            keys[idx] = key;
            vals[idx] = 1;
        } else {
            ++vals[idx];
        }
    }
    long long sum = 0;
    for (int32_t i = 0; i < ops; ++i) {
        seed = seed * 1664525u + 1013904223u;
        int32_t key;
        if ((i & 1) == 0) {
            key = probe[i];
        } else {
            key = static_cast<int32_t>(seed & 0x7fffffffu);
        }
        uint32_t h = static_cast<uint32_t>(key) * 2654435761u;
        int32_t idx = static_cast<int32_t>(h & 0xffffffu);
        while (keys[idx] != -1 && keys[idx] != key) idx = (idx + 1) & mask;
        if (keys[idx] == key) sum += static_cast<long long>(vals[idx]);
    }
    std::printf("%lld\n", sum);
    std::free(keys);
    std::free(vals);
    std::free(probe);
    return 0;
}
