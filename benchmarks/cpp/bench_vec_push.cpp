// C++ reference for benchmarks/cdd/bench_vec_push.cdd.
#include <cstdio>
#include <vector>
#include <cstdint>

int main() {
    std::vector<int32_t> v;
    for (int32_t i = 0; i < 32000000; ++i) {
        v.push_back(i * 3);
    }
    long long sum = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        sum += static_cast<long long>(v[i]);
    }
    std::printf("%lld\n", sum);
    return 0;
}
