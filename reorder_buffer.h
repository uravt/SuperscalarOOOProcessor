#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <array>
#include "prf.h"
#include "config.h"

struct ROBEntry {
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
        int insert(int dest_arch_reg, int dest_phys_reg, int old_phys_reg);
        bool commit(PhysicalRegisterFile& prf);
        bool isFull();
        void set_ready(int index);
};

#endif