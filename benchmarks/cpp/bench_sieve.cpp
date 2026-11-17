// C++ reference for benchmarks/cdd/bench_sieve.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    const int32_t n = 200000000;
    uint8_t* s = static_cast<uint8_t*>(std::malloc(n + 1));
    for (int32_t i = 0; i <= n; ++i) s[i] = 1;
    s[0] = 0;
    s[1] = 0;
    for (int32_t i = 2; i * i <= n; ++i) {
        if (s[i] == 1) {
            for (int32_t j = i * i; j <= n; j += i) s[j] = 0;
        }
    }
    int32_t count = 0;
    for (int32_t i = 0; i <= n; ++i) count += s[i];
    std::printf("%d\n", count);
    std::free(s);
    return 0;
}
