//
// Created by peter on 2026/9/21.
//

#include "table.h"

#include <utility>

void* Table::add(void *ptr, std::size_t size) noexcept {
    this->nitems_ ++;
    this->max_ptr_ = reinterpret_cast<uintptr_t>(ptr) + size  > this->max_ptr_ ? reinterpret_cast<uintptr_t>(ptr) + size  : this->max_ptr_;
    this->min_ptr_ = reinterpret_cast<uintptr_t>(ptr) < this->min_ptr_ ? reinterpret_cast<uintptr_t>(ptr) : this->min_ptr_;

}

void Table::rehash(const std::size_t new_nslots) {
    for (std::vector<Entry> old = std::exchange(this->items_, std::vector<Entry>(new_nslots)); auto &e : old) {
        if (!e.empty()) {
            insert_no_grow(e);
        }
    }
}

void Table::insert_no_grow(Entry &e) noexcept{
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

        i = (i + 1) & (this -> items_.size() - 1);
        dist += 1;
    }
}
