//
// Created by peter on 2026/9/21.
//

#include "table.h"

#include <utility>

constexpr std::size_t Table::hash(const void *ptr) noexcept {
    const auto ad = reinterpret_cast<uintptr_t>(ptr);
    return ad * 11400714819323198485ULL;
}

std::size_t Table::probe(const std::size_t i, const std::size_t j) const noexcept {
    return (i - j + 1) & (this->items_.size() - 1);
}

std::size_t Table::ideal_size(const std::size_t n) const noexcept {
    auto target = static_cast<std::size_t>(static_cast<double>(n + 1) / this->load_factor_);
    target = std::max<std::size_t>(target, 8);

    return std::bit_ceil(target);
}

void Table::rehash(const std::size_t new_nslots) {
    for (const std::vector<Entry> old = std::exchange(items_, std::vector<Entry>(new_nslots)); const Entry& e: old) {
        if (!e.empty()) {
            insert_no_grow(e);
        }
    }
}

void Table::insert_no_grow(const Entry &e) noexcept{
    std::size_t i = ideal_slot(e.hash);
    std::size_t dist = 0;
    while (true) {
        Entry &cur = items_[i];
        if (cur.empty()) {
            cur = e;
            return;
        }

        if (probe(i, cur.hash) < dist) {
            dist = probe(i, cur.hash);
            std::swap(this->items_[i], cur);
        }

        i = (i + 1) & (this -> items_.size() - 1);
        dist += 1;
    }
}

std::size_t Table::ideal_slot(const std::size_t n) const noexcept {
    constexpr int bits = std::numeric_limits<std::size_t>::digits;
    return n >> (bits - std::countr_zero(this->items_.size()));
}
