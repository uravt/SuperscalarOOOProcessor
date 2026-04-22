#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <array>
#include "prf.h"
#include "config.h"

struct ROBEntry {
    uint64_t seq;
    int dest_arch_reg;
    int dest_phys_reg;
    int old_phys_reg;
    bool completed;
};

class ReorderBuffer {
    private:
        std::array<ROBEntry, config::REORDER_BUFFER_SIZE> buffer;
        int size;
        int head;
        int tail;
    public:
        ReorderBuffer();
        int insert(uint64_t seq, int dest_arch_reg, int dest_phys_reg, int old_phys_reg);
        bool commit(PhysicalRegisterFile& prf);
        bool full();
        void set_ready(int index);
        void squash(uint64_t branch_seq, PhysicalRegisterFile& prf);
};

#endif