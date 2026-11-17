// C++ reference for benchmarks/cdd/bench_binary_search.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    const int32_t n = 16777216;
    int32_t* a = static_cast<int32_t*>(std::malloc(n * 4));
    for (int32_t i = 0; i < n; ++i) a[i] = i * 2;
    uint32_t seed = 123456789u;
    long long sum = 0;
    for (int32_t q = 0; q < 2000000; ++q) {
        seed = seed * 1664525u + 1013904223u;
        int32_t key = static_cast<int32_t>(seed & 0x1ffffffu);
        int32_t lo = 0;
        int32_t hi = n - 1;
        int32_t res = -1;
        while (lo <= hi) {
            int32_t mid = (lo + hi) >> 1;
            int32_t v = a[mid];
            if (v == key) {
                res = mid;
                break;
            }
            if (v < key) lo = mid + 1;
            else hi = mid - 1;
        }
        sum += static_cast<long long>(res);
    }
    std::printf("%lld\n", sum);
    std::free(a);
    return 0;
}
