/**
 * @file ArenaTest.cpp
 * @brief Unit tests for ArenaAllocator.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "util/Arena.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        std::printf("  ok   - %s\n", msg);
    } else {
        std::printf(" FAIL - %s\n", msg);
        ++g_failures;
    }
}

} // namespace

int main() {
    std::printf("ArenaAllocator tests\n");

    util::ArenaAllocator arena;

    void* p1 = arena.allocate(64);
    check(p1 != nullptr, "allocate returns non-null");
    check(arena.owns(p1), "owns() returns true for allocated pointer");
    check(!arena.owns(nullptr), "owns() returns false for null");

    arena.reset();
    check(arena.capacityRemaining() == arena.totalCapacity(), "reset() frees all allocations");

    void* p2 = arena.allocate(1024);
    check(p2 != nullptr, "allocate after reset returns non-null");
    check(arena.owns(p2), "owns() true after reset+allocate");

    for (int i = 0; i < 10; ++i) {
        arena.allocate(4096);
    }
    size_t capBefore = arena.totalCapacity();
    arena.reset();
    check(arena.capacityRemaining() == arena.totalCapacity(), "reset() after multiple chunks frees all");

    if (g_failures == 0) {
        std::printf("All ArenaAllocator tests passed.\n");
        return 0;
    }
    std::printf("%d ArenaAllocator test(s) FAILED.\n", g_failures);
    return 1;
}
