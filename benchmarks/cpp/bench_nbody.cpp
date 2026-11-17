// C++ reference for benchmarks/cdd/bench_nbody.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>

int main() {
    const int32_t n = 5;
    const int32_t steps = 5000000;
    const double dt = 0.001;
    double* x = static_cast<double*>(std::malloc(n * 8));
    double* y = static_cast<double*>(std::malloc(n * 8));
    double* z = static_cast<double*>(std::malloc(n * 8));
    double* vx = static_cast<double*>(std::malloc(n * 8));
    double* vy = static_cast<double*>(std::malloc(n * 8));
    double* vz = static_cast<double*>(std::malloc(n * 8));
    double* mass = static_cast<double*>(std::malloc(n * 8));

    x[0] = 0.0; y[0] = 0.0; z[0] = 0.0; mass[0] = 39.478;
    x[1] = 4.84; y[1] = -1.16; z[1] = -0.10; mass[1] = 0.037;
    x[2] = 8.34; y[2] = 4.12; z[2] = -0.40; mass[2] = 0.011;
    x[3] = 12.89; y[3] = -15.11; z[3] = -0.22; mass[3] = 0.0017;
    x[4] = 15.37; y[4] = -25.91; z[4] = 0.17; mass[4] = 0.0020;
    for (int32_t i = 0; i < n; ++i) { vx[i] = 0.0; vy[i] = 0.0; vz[i] = 0.0; }
    vx[1] = 1.61; vy[1] = 0.55; vz[1] = -0.03;
    vx[2] = 1.49; vy[2] = 0.88; vz[2] = -0.03;
    vx[3] = 1.26; vy[3] = 0.87; vz[3] = -0.02;
    vx[4] = 1.17; vy[4] = 0.73; vz[4] = -0.01;

    for (int32_t step = 0; step < steps; ++step) {
        for (int32_t i = 0; i < n; ++i) {
            for (int32_t j = i + 1; j < n; ++j) {
                double dx = x[i] - x[j];
                double dy = y[i] - y[j];
                double dz = z[i] - z[j];
                double d2 = dx * dx + dy * dy + dz * dz;
                double invd = 1.0 / std::sqrt(d2);
                double invd3 = invd * invd * invd;
                double mi = mass[i] * invd3;
                double mj = mass[j] * invd3;
                vx[i] -= dx * mj; vy[i] -= dy * mj; vz[i] -= dz * mj;
                vx[j] += dx * mi; vy[j] += dy * mi; vz[j] += dz * mi;
            }
        }
        for (int32_t i = 0; i < n; ++i) {
            x[i] += dt * vx[i];
            y[i] += dt * vy[i];
            z[i] += dt * vz[i];
        }
    }

    double energy = 0.0;
    for (int32_t i = 0; i < n; ++i) {
        energy += 0.5 * mass[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
        for (int32_t j = i + 1; j < n; ++j) {
            double dx = x[i] - x[j];
            double dy = y[i] - y[j];
            double dz = z[i] - z[j];
            energy -= mass[i] * mass[j] / std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    std::printf("%lld\n", static_cast<long long>(energy * 1000000.0));
    std::free(x); std::free(y); std::free(z);
    std::free(vx); std::free(vy); std::free(vz);
    std::free(mass);
    return 0;
}
