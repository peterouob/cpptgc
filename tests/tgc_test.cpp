#include <cpptgc.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <expected>
#include <queue>
#include <random>
#include <vector>

namespace {

struct TestCollector : Collector {
  using Collector::clear_mark;
  using Collector::Collector;
  using Collector::drain;
  using Collector::mark_root;

  void mark_from_roots() {
    clear_mark();
    mark_root();
    drain();
  }
};

void* alloc_zeroed(Collector& gc, std::size_t size, Flags flags = Flags::None) {
  void* p = gc.alloc(size, flags).value();
  std::memset(p, 0, size);
  return p;
}

void link(void* obj, std::size_t slot, const void* target) {
  std::memcpy(static_cast<std::byte*>(obj) + slot * sizeof(void*), &target,
              sizeof target);
}

constexpr std::size_t kWords = 4;
constexpr std::size_t kObjSize = kWords * sizeof(void*);

}  // namespace

static_assert(has(Flags::Root | Flags::Leaf, Flags::Leaf));
static_assert(!has(Flags::Root, Flags::Mark));
static_assert((Flags::None | Flags::Mark) == Flags::Mark);
static_assert(((Flags::Root | Flags::Mark) & ~Flags::Mark) == Flags::Root);
static_assert([] {
  Flags f = Flags::Root;
  f |= Flags::Mark;
  f &= ~Flags::Root;
  return f == Flags::Mark;
}());

TEST(CollectorBasics, Bookkeeping) {
  int anchor;
  TestCollector gc(&anchor);
  EXPECT_EQ(gc.cur_size(), 0u);

  void* a = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* b = alloc_zeroed(gc, kObjSize);
  EXPECT_EQ(gc.cur_size(), 2u);

  ASSERT_TRUE(gc.flags_of(a).has_value());
  EXPECT_EQ(*gc.flags_of(a), Flags::Root);
  ASSERT_TRUE(gc.flags_of(b).has_value());
  EXPECT_EQ(*gc.flags_of(b), Flags::None);

  gc.free(a);
  EXPECT_EQ(gc.cur_size(), 1u);
  EXPECT_FALSE(gc.flags_of(a).has_value());

  gc.free(nullptr);
  EXPECT_EQ(gc.cur_size(), 1u);
}

TEST(CollectorBasics, AllocZeroIsValid) {
  int anchor;
  TestCollector gc(&anchor);

  const std::expected<void*, std::errc> p = gc.alloc(0, Flags::None);
  ASSERT_TRUE(p.has_value());
  EXPECT_NE(*p, nullptr);
  EXPECT_TRUE(gc.flags_of(*p).has_value());
}

TEST(CollectorRoots, RootContentIsMarked) {
  int anchor;
  TestCollector gc(&anchor);

  void* root = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* child = alloc_zeroed(gc, kObjSize);
  void* lone = alloc_zeroed(gc, kObjSize);
  link(root, 0, child);

  gc.mark_from_roots();

  EXPECT_TRUE(gc.is_marked(root));
  EXPECT_TRUE(gc.is_marked(child));
  EXPECT_FALSE(gc.is_marked(lone));
}

TEST(CollectorRoots, TransitiveChain) {
  int anchor;
  TestCollector gc(&anchor);

  void* root = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* a = alloc_zeroed(gc, kObjSize);
  void* b = alloc_zeroed(gc, kObjSize);
  void* c = alloc_zeroed(gc, kObjSize);
  link(root, 0, a);
  link(a, 2, b);
  link(b, 3, c);

  gc.mark_from_roots();

  EXPECT_TRUE(gc.is_marked(root));
  EXPECT_TRUE(gc.is_marked(a));
  EXPECT_TRUE(gc.is_marked(b));
  EXPECT_TRUE(gc.is_marked(c));
}

TEST(CollectorRoots, LeafContentNotScanned) {
  int anchor;
  TestCollector gc(&anchor);

  void* root = alloc_zeroed(gc, kObjSize, Flags::Root | Flags::Leaf);
  void* child = alloc_zeroed(gc, kObjSize);
  link(root, 0, child);

  gc.mark_from_roots();

  EXPECT_TRUE(gc.is_marked(root));
  EXPECT_FALSE(gc.is_marked(child));
}

TEST(CollectorRoots, CycleTerminates) {
  int anchor;
  TestCollector gc(&anchor);

  void* a = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* b = alloc_zeroed(gc, kObjSize);
  link(a, 0, b);
  link(b, 0, a);

  gc.mark_from_roots();

  EXPECT_TRUE(gc.is_marked(a));
  EXPECT_TRUE(gc.is_marked(b));
}

TEST(CollectorRoots, InteriorPointerIgnored) {
  int anchor;
  TestCollector gc(&anchor);

  void* root = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* child = alloc_zeroed(gc, kObjSize);
  link(root, 0, static_cast<std::byte*>(child) + 8);

  gc.mark_from_roots();

  EXPECT_FALSE(gc.is_marked(child));
}

TEST(CollectorRoots, MarkDoesNotChangeMembership) {
  int anchor;
  TestCollector gc(&anchor);

  std::vector<void*> objs;
  objs.push_back(alloc_zeroed(gc, kObjSize, Flags::Root));
  for (int i = 0; i < 100; ++i) {
    objs.push_back(alloc_zeroed(gc, kObjSize));
    link(objs[static_cast<std::size_t>(i)], 0, objs.back());
  }
  const std::size_t before = gc.cur_size();

  gc.mark_from_roots();

  EXPECT_EQ(gc.cur_size(), before);
  for (void* p : objs) {
    EXPECT_TRUE(gc.flags_of(p).has_value());
  }
}

TEST(CollectorRoots, RemarkClearsOldMarks) {
  int anchor;
  TestCollector gc(&anchor);

  void* root = alloc_zeroed(gc, kObjSize, Flags::Root);
  void* child = alloc_zeroed(gc, kObjSize);
  link(root, 0, child);

  gc.mark_from_roots();
  ASSERT_TRUE(gc.is_marked(child));

  link(root, 0, nullptr);
  gc.mark_from_roots();

  EXPECT_TRUE(gc.is_marked(root));
  EXPECT_FALSE(gc.is_marked(child));
}

TEST(CollectorRoots, DeepChainNoStackOverflow) {
  int anchor;
  TestCollector gc(&anchor);

  constexpr std::size_t kLen = 1'000'000;
  std::vector<void*> nodes(kLen);
  nodes[0] = alloc_zeroed(gc, sizeof(void*), Flags::Root);
  for (std::size_t i = 1; i < kLen; ++i) {
    nodes[i] = alloc_zeroed(gc, sizeof(void*));
    link(nodes[i - 1], 0, nodes[i]);
  }

  gc.mark_from_roots();

  std::size_t unmarked = 0;
  for (void* p : nodes) {
    if (!gc.is_marked(p)) ++unmarked;
  }
  EXPECT_EQ(unmarked, 0u);
}

class CollectorRandomGraph : public ::testing::TestWithParam<std::uint64_t> {};

TEST_P(CollectorRandomGraph, MarksExactlyReachable) {
  int anchor;
  TestCollector gc(&anchor);
  std::mt19937_64 rng(GetParam());

  constexpr std::size_t kN = 2000;
  std::vector<void*> objs(kN);
  std::vector<bool> is_root(kN), is_leaf(kN);
  std::vector<std::vector<std::size_t>> edges(kN);

  std::bernoulli_distribution root_dist(0.02);
  std::bernoulli_distribution leaf_dist(0.10);
  std::bernoulli_distribution edge_dist(0.30);
  std::bernoulli_distribution interior_dist(0.05);
  std::uniform_int_distribution<std::size_t> pick(0, kN - 1);

  for (std::size_t i = 0; i < kN; ++i) {
    is_root[i] = root_dist(rng);
    is_leaf[i] = leaf_dist(rng);
    Flags f = Flags::None;
    if (is_root[i]) f |= Flags::Root;
    if (is_leaf[i]) f |= Flags::Leaf;
    objs[i] = alloc_zeroed(gc, kObjSize, f);
  }

  for (std::size_t i = 0; i < kN; ++i) {
    for (std::size_t w = 0; w < kWords; ++w) {
      if (!edge_dist(rng)) continue;
      const std::size_t j = pick(rng);
      if (interior_dist(rng)) {
        link(objs[i], w, static_cast<std::byte*>(objs[j]) + 8);
      } else {
        link(objs[i], w, objs[j]);
        edges[i].push_back(j);
      }
    }
  }

  std::vector<bool> expected(kN, false);
  std::queue<std::size_t> q;
  for (std::size_t i = 0; i < kN; ++i) {
    if (is_root[i]) {
      expected[i] = true;
      q.push(i);
    }
  }
  while (!q.empty()) {
    const std::size_t i = q.front();
    q.pop();
    if (is_leaf[i]) continue;
    for (const std::size_t j : edges[i]) {
      if (!expected[j]) {
        expected[j] = true;
        q.push(j);
      }
    }
  }

  const auto reachable = static_cast<std::size_t>(
      std::count(expected.begin(), expected.end(), true));
  ASSERT_GT(reachable, 0u);
  ASSERT_LT(reachable, kN);

  gc.mark_from_roots();

  std::size_t mismatches = 0;
  for (std::size_t i = 0; i < kN && mismatches < 10; ++i) {
    if (gc.is_marked(objs[i]) != expected[i]) {
      ++mismatches;
      ADD_FAILURE() << "object " << i << ": marked=" << gc.is_marked(objs[i])
                    << " expected=" << expected[i] << " root=" << is_root[i]
                    << " leaf=" << is_leaf[i];
    }
  }
  EXPECT_EQ(mismatches, 0u);
}

INSTANTIATE_TEST_SUITE_P(Seeds, CollectorRandomGraph,
                         ::testing::Values(1, 2, 3, 42, 20260922));

namespace {

[[gnu::noinline]] void stack_case(TestCollector& gc) {
  void* obj = alloc_zeroed(gc, kObjSize);
  void* child = alloc_zeroed(gc, kObjSize);
  link(obj, 1, child);

  gc.mark();

  EXPECT_TRUE(gc.is_marked(obj));
  EXPECT_TRUE(gc.is_marked(child));
}

}  // namespace

TEST(CollectorStack, LiveLocalIsMarked) {
  int anchor;
  TestCollector gc(&anchor);
  stack_case(gc);
}