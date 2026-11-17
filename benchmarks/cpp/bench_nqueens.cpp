// C++ reference for benchmarks/cdd/bench_nqueens.cdd.
#include <cstdio>
#include <cstdint>
#include <cstdlib>

static int32_t solutions = 0;

static bool safe(int32_t* cols, int32_t row, int32_t col) {
    for (int32_t i = 0; i < row; ++i) {
        int32_t c = cols[i];
        if (c == col) return false;
        int32_t d = row - i;
        if (c - col == d || col - c == d) return false;
    }
    return true;
}

static void solve(int32_t* cols, int32_t row, int32_t n) {
    if (row == n) {
        ++solutions;
        return;
    }
    for (int32_t c = 0; c < n; ++c) {
        if (safe(cols, row, c)) {
            cols[row] = c;
            solve(cols, row + 1, n);
        }
    }
}

int main() {
    const int32_t n = 13;
    int32_t* cols = static_cast<int32_t*>(std::malloc(n * 4));
    solutions = 0;
    solve(cols, 0, n);
    std::printf("%d\n", solutions);
    std::free(cols);
    return 0;
}
