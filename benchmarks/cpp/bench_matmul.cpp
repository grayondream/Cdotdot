// C++ reference for benchmarks/cdd/bench_matmul.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main() {
    const int32_t n = 768;
    double* a = static_cast<double*>(std::malloc(n * n * 8));
    double* b = static_cast<double*>(std::malloc(n * n * 8));
    double* c = static_cast<double*>(std::malloc(n * n * 8));
    for (int32_t i = 0; i < n * n; ++i) {
        a[i] = static_cast<double>(i % 7) * 0.5;
        b[i] = static_cast<double>(i % 11) * 0.25;
    }
    for (int32_t i = 0; i < n; ++i) {
        for (int32_t j = 0; j < n; ++j) c[i * n + j] = 0.0;
        for (int32_t k = 0; k < n; ++k) {
            double aik = a[i * n + k];
            for (int32_t j = 0; j < n; ++j) {
                c[i * n + j] = c[i * n + j] + aik * b[k * n + j];
            }
        }
    }
    std::printf("%lld\n", static_cast<long long>(c[0] + c[n * n - 1]));
    std::free(a);
    std::free(b);
    std::free(c);
    return 0;
}
