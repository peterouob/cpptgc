//
// Created by peter on 2026/9/21.
//

#include "table.h"

#include <cassert>
#include <utility>

void Table::add(void *ptr, std::size_t size) {
    if (nslots() == 0 || static_cast<double>(nitems_ + 1) > static_cast<double>(nslots()) * load_factor_)
        grow();

    assert(ptr != nullptr && "Table::add: ptr must not be null");
    insert_no_grow(Entry{ptr, size,hash(ptr)});

    nitems_++;
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    min_ptr_ = std::min(min_ptr_, addr);
    max_ptr_ = std::max(max_ptr_, addr + size);
}

void Table::remove(const void *ptr) noexcept {
    const auto *target = find(ptr);
    if (target == nullptr) {
        return;
    }

    auto i = static_cast<std::size_t>(target - items_.data());
    items_[i] = Entry{};

    std::size_t j = (i + 1) & mask();

    while (!items_[j].empty() && probe(j, items_[j].hash) > 0) {
        items_[i] = items_[j];
        items_[j] = Entry{};
        i = j;
        j = (j + 1) & mask();
    }

    nitems_ -= 1;
    shrink();
}

void Table::rehash(const std::size_t new_nslots) {
    for (const std::vector<Entry> old = std::exchange(items_, std::vector<Entry>(new_nslots));
        auto &e : old) {
        if (!e.empty())
            insert_no_grow(e);
    }
}

void Table::insert_no_grow(Entry e) noexcept{
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
            std::swap(e, cur);
        }

        i = (i + 1) & mask();
        dist += 1;
    }
}
