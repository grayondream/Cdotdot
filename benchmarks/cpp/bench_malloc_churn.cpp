// C++ reference for benchmarks/cdd/bench_malloc_churn.cdd.
// The C++ side uses the platform allocator; CDD programs link mimalloc.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    const int32_t n = 6000000;
    uint8_t** ptrs = static_cast<uint8_t**>(std::malloc(n * 8));
    long long sum = 0;
    for (int32_t i = 0; i < n; ++i) {
        uint8_t* p = static_cast<uint8_t*>(std::malloc(64));
        p[0] = static_cast<uint8_t>(i & 127);
        p[63] = static_cast<uint8_t>((i * 3) & 127);
        ptrs[i] = p;
        sum += static_cast<long long>(p[0]) + static_cast<long long>(p[63]);
    }
    for (int32_t k = 0; k < n; ++k) {
        // 131 is coprime with n = 2^7 * 5^6, so this index mapping is a
        // permutation: every block is freed exactly once.
        int32_t idx = (k * 131) % n;
        uint8_t* p = ptrs[idx];
        sum += static_cast<long long>(p[0]) + static_cast<long long>(p[63]);
        std::free(p);
    }
    std::printf("%lld\n", sum);
    std::free(ptrs);
    return 0;
}
