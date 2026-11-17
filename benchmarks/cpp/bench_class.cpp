// C++ reference for benchmarks/cdd/bench_class.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

class IVec3 {
public:
    int32_t x;
    int32_t y;
    int32_t z;

    void set(int32_t a, int32_t b, int32_t c) {
        x = a;
        y = b;
        z = c;
    }

    void add(IVec3 o) {
        x = (x + o.x) & 1023;
        y = (y + o.y) & 1023;
        z = (z + o.z) & 1023;
    }

    int64_t dot(IVec3 o) const {
        return static_cast<int64_t>(x) * static_cast<int64_t>(o.x) +
               static_cast<int64_t>(y) * static_cast<int64_t>(o.y) +
               static_cast<int64_t>(z) * static_cast<int64_t>(o.z);
    }
};

int main() {
    const int32_t n = 4000000;
    IVec3* v = static_cast<IVec3*>(std::malloc(n * 12));
    for (int32_t i = 0; i < n; ++i) {
        v[i].set(i & 1023, (i * 3) & 1023, (i * 7) & 1023);
    }
    for (int32_t pass = 0; pass < 64; ++pass) {
        for (int32_t i = 0; i < n; ++i) {
            int32_t next = i + 1;
            if (next == n) next = 0;
            v[i].add(v[next]);
        }
    }
    int64_t sum = 0;
    for (int32_t i = 0; i < n; ++i) {
        sum += v[i].dot(v[i]);
    }
    std::printf("%lld\n", static_cast<long long>(sum));
    std::free(v);
    return 0;
}
