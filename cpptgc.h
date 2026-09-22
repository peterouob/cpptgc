//
// Created by peter on 2026/9/18.
//

#ifndef CPPTGC_CPPTGC_H
#define CPPTGC_CPPTGC_H
#include <cassert>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>

#include "table.h"

class Collector {
 public:
  explicit Collector(const void* stack_bottom) noexcept
      : stack_bottom_(stack_bottom) {
    assert(stack_bottom != nullptr &&
           "Collector::Collector: stack_bottom must not be null");
  }

  ~Collector() {
    for (const Entry& e : table_.slots())
      if (!e.empty()) std::free(e.ptr);
  };

  Collector(const Collector&) =
      delete ("collector owns heap state; copy would double-free");
  Collector& operator=(const Collector&) =
      delete ("collector owns heap state; copy would double-free");
  Collector(Collector&&) =
      delete ("stack scanning ties the collector to a fixed address");
  Collector& operator=(const Collector&&) =
      delete ("stack scanning ties the collector to a fixed address");

  [[nodiscard]] std::expected<void*, std::errc> alloc(
      std::size_t size, Flags flags = Flags::None);
  void free(void* ptr) noexcept;

  void mark();

  [[nodiscard]] std::optional<Flags> flags_of(const void* ptr) const noexcept {
    const Entry* e = table_.find(ptr);
    if (e == nullptr) return std::nullopt;

    return e->flags;
  }

  [[nodiscard]] bool is_marked(const void* ptr) const noexcept {
    const auto f = flags_of(ptr);
    return f.has_value() && has(*f, Flags::Mark);
  }

  [[nodiscard]] std::size_t cur_size() const noexcept { return table_.size(); }

 protected:
  void clear_mark() noexcept;

  void mark_root();
  void drain();

  void mark_cand(std::uintptr_t word);
  void scan(std::span<const std::byte> region);

  [[gnu::noinline]] void mark_stack();

 private:
  Table table_;
  const void* stack_bottom_;
  std::vector<std::span<const std::byte>> worklist_;
};

#endif  // CPPTGC_CPPTGC_H
