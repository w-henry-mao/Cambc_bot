#include "test.h"
int main() {
    int failed_tests = 0;
    for (const auto& t : test_registry()) {
        const int before = test_failures();
        t.fn();
        if (test_failures() != before) { ++failed_tests; std::printf("FAILED %s\n", t.name); }
    }
    const int total = static_cast<int>(test_registry().size());
    std::printf("%d passed, %d failed\n", total - failed_tests, failed_tests);
    return failed_tests ? 1 : 0;
}
