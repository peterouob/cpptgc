//
// Created by peter on 2026/9/21.
//
#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

#ifndef CPPTGC_TABLE_H
#define CPPTGC_TABLE_H

enum class Flags : std::uint8_t {
  None = 0,
  Mark = 1u << 0,
  Root = 1u << 1,
  Leaf = 1u << 2,
};

constexpr Flags operator|(const Flags a, const Flags b) noexcept {
  return static_cast<Flags>(std::to_underlying(a) | std::to_underlying(b));
}
constexpr Flags operator&(const Flags a, const Flags b) noexcept {
  return static_cast<Flags>(std::to_underlying(a) & std::to_underlying(b));
}
constexpr Flags operator~(const Flags a) noexcept {
  return static_cast<Flags>(static_cast<std::uint8_t>(~std::to_underlying(a)));
}
constexpr Flags& operator|=(Flags& a, const Flags b) noexcept {
  return a = a | b;
}
constexpr Flags& operator&=(Flags& a, const Flags b) noexcept {
  return a = a & b;
}
constexpr bool has(const Flags set, const Flags bit) noexcept {
  return (set & bit) != Flags::None;
}

using Destructor = void (*)(void*) noexcept;

struct Entry {
  void* ptr = nullptr;
  std::size_t size = 0;
  std::size_t hash = 0;

  Flags flags = Flags::None;
  Destructor dtor = nullptr;

  [[nodiscard]] bool empty() const noexcept { return ptr == nullptr; };
};

class Table {
 public:
  Table() = default;

  void add(void* ptr, std::size_t size, Flags flags = Flags::None,
           Destructor dtor = nullptr);

  template <typename Self>
  [[nodiscard]] auto find(this Self& self, const void* ptr) noexcept
      -> decltype(&self.items_[0]) {
    if (self.items_.empty()) return nullptr;

    const std::size_t h = hash(ptr);
    std::size_t i = self.ideal_slot(h);

    std::size_t dist = 0;
    while (true) {
      auto& cur = self.items_[i];

      if (cur.empty() || self.probe(i, cur.hash) < dist) {
        return nullptr;
      }

      if (cur.hash == h && cur.ptr == ptr) {
        return &cur;
      }

      i = (i + 1) & self.mask();
      dist += 1;
    }
  }

  void remove(const void* ptr) noexcept;

  /* For test so public this makes test easy */
  [[nodiscard]] std::size_t size() const noexcept { return nitems_; }
  [[nodiscard]] std::size_t nslots() const noexcept { return items_.size(); }
  [[nodiscard]] bool empty() const noexcept { return nitems_ == 0; }
  [[nodiscard]] std::uintptr_t min_ptr() const noexcept { return min_ptr_; }
  [[nodiscard]] std::uintptr_t max_ptr() const noexcept { return max_ptr_; }

  template <typename Self>
  [[nodiscard]] auto slots(this Self& self) noexcept {
    return std::span(self.items_);
  }

 protected:
  [[nodiscard]] static std::size_t hash(const void* ptr) noexcept {
    const auto ad = reinterpret_cast<uintptr_t>(ptr);
    return ad * 11400714819323198485ULL;
  }

  [[nodiscard]] std::size_t ideal_slot(const std::size_t n) const noexcept {
    constexpr int bits = std::numeric_limits<std::size_t>::digits;
    return n >> (bits - std::countr_zero(nslots()));
  }

  [[nodiscard]] std::size_t probe(const std::size_t i,
                                  const std::size_t j) const noexcept {
    return (i - ideal_slot(j)) & mask();
  }

  [[nodiscard]] std::size_t ideal_size(std::size_t n) const noexcept {
    auto target =
        static_cast<std::size_t>(static_cast<double>(n + 1) / load_factor_);
    target = std::max<std::size_t>(target, 8);

    return std::bit_ceil(target);
  }

  [[nodiscard]] std::size_t mask() const noexcept { return items_.size() - 1; }

 private:
  void grow() { rehash(ideal_size(nitems_ + 1)); };

  void shrink() noexcept {
    const bool too_empty = static_cast<double>(nitems_) <
                           static_cast<double>(nslots()) * load_factor_ / 4;

    if (nslots() > 8 && too_empty) {
      try {
        rehash(nslots() / 2);
      } catch (const std::bad_alloc&) {
      }
    }
  };

  void rehash(std::size_t new_nSlots);
  void insert_no_grow(Entry e) noexcept;

  std::vector<Entry> items_;
  std::size_t nitems_ = 0;
  double load_factor_ = 0.9;
  std::uintptr_t min_ptr_ = UINTPTR_MAX;
  std::uintptr_t max_ptr_ = 0;
};

#endif  // CPPTGC_TABLE_H
