// C++ reference for benchmarks/cdd/bench_str_append.cdd.
#include <cstdio>
#include <string>
#include <cstdint>

int main() {
    std::string s;
    for (int32_t i = 0; i < 8000000; ++i) {
        s.append("0123456789abcdef");
    }
    long long sum = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        sum += static_cast<long long>(s[i]);
    }
    std::printf("%lld\n", sum);
    return 0;
}
