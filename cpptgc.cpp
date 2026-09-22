//
// Created by peter on 2026/9/18.
//

#include "cpptgc.h"

#include <cassert>
#include <csetjmp>
#include <cstring>
#include <memory>

#if defined(__has_builtin)
#if __has_builtin(__builtin_unwind_init)
#define TGC_HAS_UNWIND_INIT 1
#endif
#endif

void Collector::mark() {
  clear_mark();
  mark_root();
  drain();

#ifdef TGC_HAS_UNWIND_INIT
  __builtin_unwind_init();
#else
  // ReSharper disable once CppReplaceMemsetWithZeroInitialization
  std::jmp_buf env;
  std::memset(&env, 0, sizeof env);
  setjmp(env);  // NOLINT(cert-err52-cpp)
#endif

  void (Collector::* volatile fn)() = &Collector::mark_stack;
  (this->*fn)();

  drain();
}

void Collector::mark_root() {
  for (const Entry& entry : table_.slots()) {
    if (!entry.empty() && has(entry.flags, Flags::Root)) {
      mark_cand(reinterpret_cast<std::uintptr_t>(entry.ptr));
    }
  }
}

void Collector::mark_cand(const std::uintptr_t word) {
  if (word < table_.min_ptr() || word >= table_.max_ptr()) return;

  const auto ptr = std::bit_cast<void*>(word);
  const auto entry = table_.find(ptr);

  if (entry == nullptr || has(entry->flags, Flags::Mark)) return;

  entry->flags |= Flags::Mark;

  if (!has(entry->flags, Flags::Leaf))
    worklist_.emplace_back(static_cast<const std::byte*>(entry->ptr),
                           entry->size);
}

void Collector::drain() {
  while (!worklist_.empty()) {
    const auto region = worklist_.back();
    worklist_.pop_back();
    scan(region);
  }
}

void Collector::scan(const std::span<const std::byte> region) {
  constexpr std::size_t kSize = sizeof(std::uintptr_t);
  const std::size_t nwords = region.size() / kSize;

  for (std::size_t i = 0; i < nwords; i++) {
    std::uintptr_t word;
    std::memcpy(&word, region.data() + i * kSize, kSize);
    mark_cand(word);
  }
}

void Collector::clear_mark() noexcept {
  for (Entry& e : table_.slots())
    if (!e.empty()) e.flags &= ~Flags::Mark;
}

std::expected<void*, std::errc> Collector::alloc(std::size_t size,
                                                 Flags flags) {
  if (size == 0) size = 1;

  void* p = std::malloc(size);
  if (p == nullptr) return std::unexpected(std::errc::not_enough_memory);

  std::unique_ptr<void, decltype(&std::free)> guard(p, &std::free);
  table_.add(p, size, flags);
  return guard.release();
}

void Collector::free(void* ptr) noexcept {
  if (ptr == nullptr) return;

  assert(table_.find(ptr) != nullptr && "Collector::free: ptr not found");

  table_.remove(ptr);
  std::free(ptr);
}

void Collector::mark_stack() {
  volatile std::uintptr_t marker = 0;
  const auto top = reinterpret_cast<std::uintptr_t>(&marker);
  const auto bottom = reinterpret_cast<std::uintptr_t>(stack_bottom_);

  auto l = std::min(top, bottom);
  const auto h = std::max(top, bottom);

  l &= ~(sizeof(std::uintptr_t) - 1);
  scan(std::span(reinterpret_cast<const std::byte*>(l), h - l));
}
