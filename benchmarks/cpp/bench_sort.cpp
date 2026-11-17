// C++ reference for benchmarks/cdd/bench_sort.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

static void insertion_sort(int32_t* a, int32_t lo, int32_t hi) {
    for (int32_t i = lo + 1; i < hi; ++i) {
        int32_t v = a[i];
        int32_t j = i - 1;
        while (j >= lo && a[j] > v) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = v;
    }
}

static void quick_sort(int32_t* a, int32_t lo, int32_t hi) {
    while (hi - lo > 16) {
        int32_t mid = lo + (hi - lo) / 2;
        int32_t pivot = a[mid];
        int32_t i = lo;
        int32_t j = hi - 1;
        while (i <= j) {
            while (a[i] < pivot) ++i;
            while (a[j] > pivot) --j;
            if (i <= j) {
                int32_t t = a[i];
                a[i] = a[j];
                a[j] = t;
                ++i;
                --j;
            }
        }
        if (j - lo < hi - i) {
            quick_sort(a, lo, j + 1);
            lo = i;
        } else {
            quick_sort(a, i, hi);
            hi = j + 1;
        }
    }
    insertion_sort(a, lo, hi);
}

int main() {
    const int32_t n = 8000000;
    int32_t* a = static_cast<int32_t*>(std::malloc(n * 4));
    uint32_t seed = 123456789;
    for (int32_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        a[i] = static_cast<int32_t>(seed >> 1);
    }
    quick_sort(a, 0, n);
    long long sum = 0;
    for (int32_t i = 0; i < n; ++i) {
        sum += static_cast<long long>(a[i]);
    }
    std::printf("%lld\n", sum);
    std::free(a);
    return 0;
}
