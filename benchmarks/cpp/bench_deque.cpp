// C++ reference for benchmarks/cdd/bench_deque.cdd.
#include <cstdio>
#include <cstdint>
#include <deque>

int main() {
    std::deque<int32_t> d;
    const int32_t n = 32000000;
    for (int32_t i = 0; i < n; ++i) d.push_back(i);
    long long sum = 0;
    while (!d.empty()) {
        sum += static_cast<long long>(d.front());
        d.pop_front();
    }
    std::printf("%lld\n", sum);
    return 0;
}
