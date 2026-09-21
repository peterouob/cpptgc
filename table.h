//
// Created by peter on 2026/9/21.
//
#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

#ifndef CPPTGC_TABLE_H
#define CPPTGC_TABLE_H

namespace tgcpp {
    struct Entry {
        void* ptr = nullptr;
        std::size_t size = 0;
        std::size_t hash = 0;
    };

    class Table {
    public:
        Table() = default;
        void add(void *ptr, std::size_t size);

        template<typename Self>
        [[nodiscard]] auto find(this Self&& self, const void* ptr) noexcept;

        void remove(const void* ptrr) noexcept;

    protected:
        [[nodiscard]] static constexpr std::size_t hash(const void* ptr) noexcept;
        [[nodiscard]] std::size_t probe(std::size_t i, std::size_t j) const noexcept;
        [[nodiscard]] std::size_t ideal_size(std::size_t n) const noexcept;

    private:
        void grow();
        void shrink();
        void rehash(std::size_t new_nslots);

        std::vector<Entry> items_;
        std::size_t nitems_ = 0;
        double load_factor_ = 0.9;
        std::uintptr_t min_ptr_ = UINTPTR_MAX;
        std::uintptr_t max_ptr_ = 0;
    };
}

#endif //CPPTGC_TABLE_H
