// C++ reference for benchmarks/cdd/bench_mandelbrot.cdd.
#include <cstdio>
#include <cstdint>

int main() {
    const int32_t w = 1000;
    const int32_t h = 1000;
    const int32_t maxiter = 1000;
    long long sum = 0;
    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            double cr = -2.0 + 3.0 * static_cast<double>(x) / static_cast<double>(w);
            double ci = -1.5 + 3.0 * static_cast<double>(y) / static_cast<double>(h);
            double zr = 0.0;
            double zi = 0.0;
            int32_t iter = 0;
            while (iter < maxiter) {
                double zr2 = zr * zr;
                double zi2 = zi * zi;
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                ++iter;
            }
            sum += static_cast<long long>(iter);
        }
    }
    std::printf("%lld\n", sum);
    return 0;
}
