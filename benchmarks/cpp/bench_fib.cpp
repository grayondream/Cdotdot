// C++ reference for benchmarks/cdd/bench_fib.cdd.
#include <cstdio>
#include <cstdint>

static int32_t fib(int32_t n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    std::printf("%d\n", fib(40));
    return 0;
}
