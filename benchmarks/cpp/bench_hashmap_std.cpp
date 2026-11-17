// C++ reference for benchmarks/cdd/bench_hashmap_std.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

int main() {
    std::unordered_map<uint32_t, int32_t> m;
    const int32_t ops = 4000000;
    uint32_t* probe = static_cast<uint32_t*>(std::malloc(ops * 4));
    uint32_t seed = 987654321u;
    for (int32_t i = 0; i < ops; ++i) {
        seed = seed * 1664525u + 1013904223u;
        probe[i] = seed;
        m[seed] = i;
    }
    long long sum = 0;
    for (int32_t i = 0; i < ops; ++i) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t key = probe[i];
        if ((i & 1) != 0) key = seed;
        auto it = m.find(key);
        if (it != m.end()) sum += static_cast<long long>(it->second);
    }
    std::printf("%lld\n", sum);
    std::free(probe);
    return 0;
}
