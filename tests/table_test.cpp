#include "table.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace {
    struct TestTable: Table {
        using Table::hash;
        using Table::ideal_slot;
        using Table::probe;
    };

    constexpr double kLoadFactor = 0.9;

    void* fake_ptr(std::uintptr_t id) {
        return reinterpret_cast<void*>(0x10000 + id * 16);
    }

    void check_invariants(const TestTable& t) {
        const auto& slots = t.slots();
        const std::size_t n = slots.size();

        if (n != 0) {
            ASSERT_GE(n, 8u);
            ASSERT_TRUE(std::has_single_bit(n));
            ASSERT_LE(static_cast<double>(t.size()), static_cast<double>(n) * kLoadFactor);
        }

        const auto occupied = std::ranges::count_if(slots, [](const Entry& e) {return !e.empty();});
        ASSERT_EQ(static_cast<std::size_t>(occupied), t.size());

        for (std::size_t i = 0; i < n; ++i) {
            const Entry& e = slots[i];
            if (e.empty()) continue;

            ASSERT_EQ(e.hash, TestTable::hash(e.ptr)) << "stable hash at slot" << i;

            const std::size_t home = t.ideal_slot(e.hash);
            const std::size_t dist = t.probe(i, e.hash);
            for (std::size_t k = 0; k < dist; ++k) {
                ASSERT_FALSE(slots[(home+k) & (n-1)].empty()) << "gap before slot" << i;
            }

            const std::size_t j = (i + 1) & (n - 1);
            if (!slots[j].empty()) {
                ASSERT_LE(t.probe(j, slots[j].hash), dist+1) << "robin hood violated at slot" << i;
            }

            const auto addr = reinterpret_cast<uintptr_t>(e.ptr);
            ASSERT_LE(t.min_ptr(), addr);
            ASSERT_GE(t.max_ptr(), addr + e.size);
        }
    }
}

TEST(Table, EmptyTable) {
    TestTable t;
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.find(fake_ptr(1)), nullptr);
    t.remove(fake_ptr(1));
    ASSERT_NO_FATAL_FAILURE(check_invariants(t));
}

TEST(Table, AddThenFind) {
    TestTable t;
    t.add(fake_ptr(1), 10);
    t.add(fake_ptr(2), 20);

    const Entry* e = t.find(fake_ptr(2));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->size, 20u);
    EXPECT_EQ(t.find(fake_ptr(3)), nullptr);
    ASSERT_NO_FATAL_FAILURE(check_invariants(t));
}

TEST(Table, RemoveThenNotFound) {
    TestTable t;
    for (std::uintptr_t id = 0; id < 5; ++id) t.add(fake_ptr(id), 8);

    t.remove(fake_ptr(2));
    EXPECT_EQ(t.find(fake_ptr(2)), nullptr);
    EXPECT_NE(t.find(fake_ptr(3)), nullptr);
    EXPECT_EQ(t.size(), 4u);
    ASSERT_NO_FATAL_FAILURE(check_invariants(t));
}

TEST(Table, GrowKeepsAllEntries) {
    TestTable t;
    for (std::uintptr_t id = 0; id < 1000; ++id) t.add(fake_ptr(id), id + 1);

    for (std::uintptr_t id = 0; id < 1000; ++id) {
        const Entry* e = t.find(fake_ptr(id));
        ASSERT_NE(e, nullptr) << "lost id " << id;
        EXPECT_EQ(e->size, id + 1);
    }
    ASSERT_NO_FATAL_FAILURE(check_invariants(t));
}

TEST(Table, ShrinkBackToMinimum) {
    TestTable t;
    for (std::uintptr_t id = 0; id < 1000; ++id) t.add(fake_ptr(id), 8);
    for (std::uintptr_t id = 0; id < 1000; ++id) t.remove(fake_ptr(id));

    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.nslots(), 8u);
    ASSERT_NO_FATAL_FAILURE(check_invariants(t));
}

TEST(Table, NoThrashingAtBoundary) {
    TestTable t;
    for (std::uintptr_t id = 0; id < 7; ++id) t.add(fake_ptr(id), 8);

    int resizes = 0;
    std::size_t last = t.nslots();
    for (int round = 0; round < 1000; ++round) {
        t.add(fake_ptr(100), 8);
        t.remove(fake_ptr(100));
        if (t.nslots() != last) {
            ++resizes;
            last = t.nslots();
        }
    }
    EXPECT_LE(resizes, 1) << "table is thrashing";
}

class TableDifferential : public ::testing::TestWithParam<std::uint64_t> {};

TEST_P(TableDifferential, MatchesUnorderedMap) {
    std::mt19937_64 rng(GetParam());
    TestTable table;
    std::unordered_map<void*, std::size_t> oracle;
    std::vector<void*> live;
    std::uintptr_t next_id = 0;

    constexpr int kOps = 1'000'000;

    for (int op = 0; op < kOps; ++op) {
        if (const auto r = rng() % 100; r < 50 || live.empty()) {
            void* p = fake_ptr(next_id++);
            const std::size_t size = 1 + rng() % 256;
            table.add(p, size);
            oracle.emplace(p, size);
            live.push_back(p);
        } else if (r < 80) {
            const std::size_t k = rng() % live.size();
            void* p = live[k];
            std::swap(live[k], live.back());
            live.pop_back();
            table.remove(p);
            oracle.erase(p);
        } else {
            void* p = fake_ptr(rng() % (next_id + 1));
            const Entry* e = table.find(p);
            if (const auto it = oracle.find(p); it == oracle.end()) {
                ASSERT_EQ(e, nullptr) << "op " << op;
            } else {
                ASSERT_NE(e, nullptr) << "op " << op;
                ASSERT_EQ(e->size, it->second);
            }
        }

        if (constexpr int kCheckEvery = 10'000; op % kCheckEvery == 0) {
            ASSERT_NO_FATAL_FAILURE(check_invariants(table)) << "op " << op;
            ASSERT_EQ(table.size(), oracle.size());
            for (const auto& [p, size] : oracle) {
                const Entry* e = table.find(p);
                ASSERT_NE(e, nullptr);
                ASSERT_EQ(e->size, size);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Seeds, TableDifferential,
                         ::testing::Values(1, 2, 3, 42, 20260921));