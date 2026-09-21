//
// Created by peter on 2026/9/21.
//
#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

#ifndef CPPTGC_TABLE_H
#define CPPTGC_TABLE_H

struct Entry {
    void* ptr = nullptr;
    std::size_t size = 0;
    std::size_t hash = 0;

    [[nodiscard]] bool empty() const noexcept {
        return ptr == nullptr ;
    };
};

class Table {
public:
    Table() = default;

    void *add(void *ptr, std::size_t size) noexcept;

    template<typename Self>
    [[nodiscard]] auto find(this Self&& self, const void* ptr) noexcept;

    void remove(const void* ptr) noexcept;

protected:
    [[nodiscard]] static std::size_t hash(const void* ptr) noexcept {
        const auto ad = reinterpret_cast<uintptr_t>(ptr);
        return ad * 11400714819323198485ULL;
    }

    [[nodiscard]] std::size_t ideal_slot(std::size_t n) const noexcept {
        constexpr int bits = std::numeric_limits<std::size_t>::digits ;
        return n >> (bits - std::countr_zero(nslots()));
    }

    [[nodiscard]] std::size_t probe(const std::size_t i, const std::size_t j) const noexcept {
        return (i - ideal_slot(j)) & mask();
    }

    [[nodiscard]] std::size_t ideal_size(std::size_t n) const noexcept {
        auto target = static_cast<std::size_t>(static_cast<double>(n + 1) / load_factor_);
        target = std::max<std::size_t>(target, 8);

        return std::bit_ceil(target);
    }

    [[nodiscard]] std::size_t nslots() const noexcept {
        return items_.size();
    }

    [[nodiscard]] std::size_t mask() const noexcept {
        return this->items_.size() - 1;
    }

private:
    void grow() {
        rehash(ideal_size(nitems_ + 1));
    };

    void shrink() {
        if (const std::size_t target = ideal_slot(nitems_); target < nslots()) {
            rehash(target);
        }
    };

    void rehash(std::size_t new_nslots);
    void insert_no_grow(Entry &e) noexcept;

    std::vector<Entry> items_;
    std::size_t nitems_ = 0;
    double load_factor_ = 0.9;
    std::uintptr_t min_ptr_ = UINTPTR_MAX;
    std::uintptr_t max_ptr_ = 0;
};

#endif //CPPTGC_TABLE_H
