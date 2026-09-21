//
// Created by peter on 2026/9/18.
//

#ifndef CPPTGC_CPPTGC_H
#define CPPTGC_CPPTGC_H

class Collector {
public:
    explicit Collector(const void* stack_bottom) noexcept;
    ~Collector();

    Collector(const Collector&) = delete("collector owns heap state; copy would double-free");
    Collector& operator=(const Collector&) = delete("collector owns heap state; copy would double-free");
    Collector(Collector&&) = delete("stack scanning ties the collector to a fixed address");
    Collector& operator=(const Collector&&) = delete("stack scanning ties the collector to a fixed address");
};

#endif //CPPTGC_CPPTGC_H
