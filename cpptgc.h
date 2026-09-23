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

  void collect();
  void pause();
  void resume();

  [[nodiscard]] bool is_pause() const noexcept;

  [[nodiscard]] std::expected<void*, std::errc> calloc(
      std::size_t n, std::size_t size, Flags flags = Flags::None,
      Destructor dtor = nullptr);
  [[nodiscard]] std::expected<void*, std::errc> realloc(void* ptr,
                                                        std::size_t size);

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

  void set_flags(void* ptr, Flags flags) noexcept;
  void set_dtor(void* ptr, Destructor dtor) noexcept;

  [[nodiscard]] std::optional<std::size_t> size_of(
      const void* ptr) const noexcept;
  [[nodiscard]] std::optional<Destructor> dtor_of(
      const void* ptr) const noexcept;

 protected:
  void clear_mark() noexcept;

  void mark_root();
  void drain();

  void mark_cand(std::uintptr_t word);
  void scan(std::span<const std::byte> region);

  [[gnu::noinline]] void mark_stack();

  [[nodiscard]] std::size_t collections() const noexcept;

  void sweep();

 private:
  Table table_;
  const void* stack_bottom_;
  std::vector<std::span<const std::byte>> worklist_;

  std::vector<Entry> free_;
  std::size_t threshold_ = 64;
  double sweep_factor_ = 0.5;
  std::size_t collections_ = 0;
  bool paused_ = false;
};

class [[nodiscard]] PauseGuard {
 public:
  explicit PauseGuard(Collector& gc) noexcept;
  ~PauseGuard();
  PauseGuard(const PauseGuard&) = delete;
  PauseGuard& operator=(const PauseGuard&) = delete;

 private:
  Collector& gc_;
  bool was_paused_;
};

#endif  // CPPTGC_CPPTGC_H
